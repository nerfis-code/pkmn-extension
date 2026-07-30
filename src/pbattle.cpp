#include "pbattle.h"
#include "jsrutime.h"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <ranges>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/grid_container.hpp>
#include <godot_cpp/classes/interval_tweener.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/method_tweener.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/variant.hpp>

extern "C" {
#include "quickjs-libc.h"
#include "quickjs.h"
}

using json = nlohmann::json;
using namespace godot;

extern "C" {
extern const uint8_t qjsc_out[];
extern const uint32_t qjsc_out_size;
}

void PBattlePeer::_bind_methods() {
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_OPEN);
	BIND_ENUM_CONSTANT(STATE_CLOSED);

	ClassDB::bind_method(D_METHOD("prepare", "player_name", "packed_team"), &PBattlePeer::prepare);
	ClassDB::bind_method(D_METHOD("start"), &PBattlePeer::start);
	ClassDB::bind_method(D_METHOD("poll"), &PBattlePeer::poll);
	ClassDB::bind_method(D_METHOD("get_ready_state"), &PBattlePeer::get_ready_state);
	ClassDB::bind_method(D_METHOD("get_available_packet_count"), &PBattlePeer::get_available_packet_count);
	ClassDB::bind_method(D_METHOD("get_packet"), &PBattlePeer::get_packet);
	ClassDB::bind_method(D_METHOD("_stdin", "input"), &PBattlePeer::_stdin);
	ClassDB::bind_method(D_METHOD("send", "packet"), &PBattlePeer::send);
}

PBattlePeer::PBattlePeer() {
	runtime_ = JS_NewRuntime();
	context_ = JS_NewContext(runtime_);

	JS_SetContextOpaque(context_, this);
}

PBattlePeer::~PBattlePeer() {
	if (context_ != nullptr) {
		JS_FreeContext(context_);
	}

	if (job_context_ != nullptr) {
		JS_FreeContext(job_context_);
	}

	if (runtime_ != nullptr) {
		js_std_free_handlers(runtime_);
		JS_FreeRuntime(runtime_);
	}
}

void PBattlePeer::prepare(godot::String player_name, godot::String packed_team) {
	JSValue global_obj = JS_GetGlobalObject(context_);
	// TODO: es buena idea borrar estos string de memoria
	JS_SetPropertyStr(context_, global_obj, "p1Name", JS_NewString(context_, player_name.utf8().get_data()));
	JS_SetPropertyStr(context_, global_obj, "p1PackedTeam", JS_NewString(context_, packed_team.utf8().get_data()));
	JS_FreeValue(context_, global_obj);

	js_std_init_handlers(runtime_);
	//js_std_add_helpers(context_, argc, argv);
	js_init_module_std(context_, "std");
	js_init_module_os(context_, "os");

	install_console(context_);

	js_std_eval_binary(context_, qjsc_out, qjsc_out_size, 0);
}

void PBattlePeer::start() {
	ready_state_ = STATE_OPEN;
}

void PBattlePeer::_stdin(String in) {
	packet_queue_.append_array(in.split("\n"));
}

void PBattlePeer::poll() {
	if (ready_state_ != STATE_OPEN) return;

	int job_ret;
	job_ret = JS_ExecutePendingJob(runtime_, &job_context_);
	if (job_ret <= 0) {
		if (job_ret < 0) {
			ready_state_ = STATE_CLOSED;
			print_exception(job_context_);
		}
		return;
	}

	/* 2. Revisar si quedan timers u operaciones de I/O pendientes (módulo os) */
	if (js_std_loop(context_) != 0) {
		ready_state_ = STATE_CLOSED;
		print_exception(context_);
	}

	/* Si no hay más timers ni jobs pendientes, salimos */
	if (!JS_IsJobPending(runtime_))
		return; // TODO: deberia cambiar su estado
}

int PBattlePeer::get_ready_state() {
	return ready_state_;
}

int PBattlePeer::get_available_packet_count() {
	return packet_queue_.size();
}

String PBattlePeer::get_packet() {
	if (packet_queue_.is_empty()) {
		return {};
	}

	String packet = packet_queue_[0];
	packet_queue_.remove_at(0);
	return packet;
}

void PBattlePeer::send(String packet) {
	JSValue global_obj = JS_GetGlobalObject(context_);
	JSValue func = JS_GetPropertyStr(context_, global_obj, "choose");

	if (JS_IsFunction(context_, func)) {
		JSValue args[1];
		args[0] = JS_NewString(context_, packet.utf8().get_data());

		JSValue result = JS_Call(context_, func, JS_UNDEFINED, 1, args);

		if (JS_IsException(result))
			print_exception(context_);

		JS_FreeValue(context_, result);
		JS_FreeValue(context_, args[0]);
	} else {
		print_error("function choose not found");
	}

	JS_FreeValue(context_, func);
	JS_FreeValue(context_, global_obj);
}

// void PBattle::handle_action(String action) {
// 	auto [args, kw_args] = parse_battle_line(action);
// 	ERR_FAIL_COND_MSG(args.size() < 1, "La longitud de args debe ser de al menos 1");

// 	if (args[0] == "request") {
// 		json request_payload = json::parse(action.substr(9).utf8().get_data());
// 		json active = request_payload["active"];

// 		if (active.is_null())
// 			return;
// 		std::vector<Move> moves = active[0]["moves"].get<std::vector<Move>>();

// 		Array moveset;
// 		for (auto move : moves) {
// 			moveset.push_back(String(move.move.c_str()));
// 		}

// 		battle_scene_->call("display_moveset", moveset);
// 	} else if (args[0] == "-damage") {
// 		ERR_FAIL_COND_MSG(args.size() < 3, "Los args del action -damage deben tener al menos una longitud de 3");

// 		String ident = args[1]; // "p1a: Bulbasaur"
// 		String position = ident.substr(0, 3);

// 		if (args[2].ends_with("fnt")) {
// 			animation("take_damage", { position, 0 });
// 			return;
// 		}

// 		auto hp_diff = args[2].split("/");

// 		int hp_max = hp_diff[1].to_int();
// 		int hp = hp_diff[0].to_int();
// 		animation("take_damage", { position, hp });

// 		if (kw_args.has("from")) {
// 			String from = kw_args["from"];
// 			if (from == "Recoil") {
// 				animation("message", { vformat("%s has taken recoil damage", ident.substr(5)) });
// 			}
// 		}
// 	} else if (args[0] == "-heal") {
// 		ERR_FAIL_COND_MSG(args.size() < 3, "Los args del action -heal deben tener al menos una longitud de 3");

// 		String ident = args[1];
// 		String position = ident.substr(0, 3);

// 		auto hp_diff = args[2].split("/");
// 		int hp = hp_diff[0].to_int();
// 		animation("take_damage", { position, hp });

// 		if (kw_args.has("from")) {
// 			String from = kw_args["from"];
// 			// ej: "item: Leftovers" → extraer el nombre del objeto
// 			if (from.begins_with("item:")) {
// 				String item_name = from.substr(5).strip_edges();
// 				animation("message", { vformat("%s restored HP with its %s!", ident.substr(5), item_name) });
// 			}
// 		}
// 	} else if (args[0] == "switch") { // |switch|p1a: Arbok|Arbok, L78, M|254/254
// 		ERR_FAIL_COND_MSG(args.size() < 4, "Los args del actions switch deben tener al menos una longitud de 4");

// 		String ident = args[1]; // p1a: Growlithe

// 		PackedStringArray hp_diff = args[3].split("/");
// 		int max_hp = hp_diff[1].to_int();
// 		int hp = hp_diff[0].to_int();
// 		battle_scene_->call("switch", ident, max_hp, hp);
// 	} else if (args[0] == "win") {
// 		animation("message", { vformat("%s win!\n", args[1]) });
// 	} else if (args[0] == "faint") {
// 		String position = args[1].substr(0, 3);
// 		animation("take_damage", { position, 0 });
// 		animation("message", { vformat("%s has fainted!\n", args[1].substr(5)) });
// 	} else if (args[0] == "error") {
// 		if (kw_args.has("Invalid choice")) {
// 			battle_scene_->call("show_options");
// 		}
// 	} else if (args[0] == "move") {
// 		String ident = args[1];
// 		String move = args[2];

// 		animation("message", { vformat("%s has used \n%s", ident, move) });
// 	} else if (args[0] == "-ability") {
// 		String ident = args[1];
// 		String pokemon = ident.substr(5);
// 		String ability = args[2];

// 		animation("splash", { pokemon, ability });
// 	} else if (args[0] == "-resisted") {
// 		animation("message", { String("It's not very effective...") });
// 	} else if (args[0] == "-supereffective") {
// 		animation("message", { String("It's very effective...") });
// 	} else if (args[0] == "-unboost") {
// 		ERR_FAIL_COND_MSG(args.size() < 4, "Los args del action -unboost deben tener al menos una longitud de 4");

// 		String pokemon = args[1].substr(5); // quitar "p1a: "
// 		String stat = args[2];
// 		String stages = args[3];

// 		animation("message", { vformat("%s's %s fell!", pokemon, stat) });
// 	} else if (args[0] == "-boost") {
// 		ERR_FAIL_COND_MSG(args.size() < 4, "Los args del action -boost deben tener al menos una longitud de 4");

// 		String pokemon = args[1].substr(5); // quitar "p1a: "
// 		String stat = args[2];
// 		String stages = args[3];

// 		animation("message", { vformat("%s's %s rose!", pokemon, stat) });
// 	} else if (args[0] == "-activate") {
// 		ERR_FAIL_COND_MSG(args.size() < 3, "Los args del action -activate deben tener al menos una longitud de 3");

// 		String pokemon = args[1].substr(5);
// 		String effect = args[2]; // "move: Protect"

// 		if (effect.begins_with("move:")) {
// 			String move_name = effect.substr(5).strip_edges();
// 			animation("message", { vformat("%s protected itself!", pokemon) });
// 		}
// 	} else if (args[0] == "-fail") {
// 		ERR_FAIL_COND_MSG(args.size() < 2, "Los args del action -fail deben tener al menos una longitud de 2");

// 		String pokemon = args[1].substr(5);

// 		if (kw_args.has("from")) {
// 			String from = kw_args["from"];
// 			// ej: "ability: Own Tempo"
// 			if (from.begins_with("ability:")) {
// 				String ability_name = from.substr(8).strip_edges();
// 				animation("message", { vformat("%s's %s prevents that!", pokemon, ability_name) });
// 			}
// 		} else {
// 			animation("message", { vformat("But it failed for %s!", pokemon) });
// 		}
// 	} else if (args[0] == "poke" || args[0] == "teampreview" || args[0] == "teamsize" || args[0] == "t:" || args[0] == "upkeep" || args[0] == "done") {
// 		// Acciones de metadata/setup que no requieren animación
// 		return;
// 	}
// }

// void PBattle::choose(String choice) {
// 	JSValue global_obj = JS_GetGlobalObject(context_);
// 	JSValue func = JS_GetPropertyStr(context_, global_obj, "choose");

// 	if (JS_IsFunction(context_, func)) {
// 		JSValue args[1];
// 		args[0] = JS_NewString(context_, choice.utf8().get_data());

// 		JSValue result = JS_Call(context_, func, JS_UNDEFINED, 1, args);

// 		if (JS_IsException(result))
// 			print_exception(context_);

// 		JS_FreeValue(context_, result);
// 		JS_FreeValue(context_, args[0]);
// 	} else {
// 		print_error("function choose not found");
// 	}

// 	JS_FreeValue(context_, func);
// 	JS_FreeValue(context_, global_obj);

// 	battle_scene_->call("hide_options");
// }

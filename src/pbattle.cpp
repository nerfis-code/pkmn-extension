#include "pbattle.h"

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

using json = nlohmann::json;
using namespace godot;

void PBattle::_bind_methods() {
	ClassDB::bind_method(D_METHOD("completed", "vr"), &PBattle::completed);
	// TODO: cambiar los nombre write y fragment por choose y choice
	ClassDB::bind_method(D_METHOD("write", "fragment"), &PBattle::write);
}

PBattle::PBattle() {}

PBattle::~PBattle() {
	if (context_ != nullptr) {
		JS_FreeContext(context_);
	}

	if (job_context_ != nullptr) {
		JS_FreeContext(job_context_);
	}

	if (runtime_ != nullptr) {
		JS_FreeRuntime(runtime_);
	}
}

std::string read_file(const char *path) {
	String ssource = FileAccess::open(String(path), FileAccess::READ)->get_as_text();
	std::string source = ssource.utf8().get_data();

	return source;
}

JSValue js_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	PBattle *sd = static_cast<PBattle *>(JS_GetContextOpaque(ctx));

	for (int i = 0; i < argc; i++) {
		const char *output = JS_ToCString(ctx, argv[i]);

		if (output == nullptr) {
			continue;
		}

		sd->handle_chunk(output);

		JS_FreeCString(ctx, output);
	}
	return JS_UNDEFINED;
}

void install_console(JSContext *ctx) {
	JSValue globalthis = JS_GetGlobalObject(ctx);
	JSValue console = JS_NewObject(ctx);
	JSValue log = JS_NewCFunction(ctx, &js_console_log, "log", 1);

	JS_SetPropertyStr(ctx, console, "log", log);
	JS_SetPropertyStr(ctx, globalthis, "console", console);
	JS_FreeValue(ctx, globalthis);
}

void print_exception(JSContext *ctx) {
	JSValue exception = JS_GetException(ctx);
	const char *error_str = JS_ToCString(ctx, exception);
	if (error_str != nullptr) {
		print_error(error_str);
		JS_FreeCString(ctx, error_str);
	}
	JS_FreeValue(ctx, exception);
}

struct Move {
	std::string move;
	std::string id;
	uint16_t maxpp;
	std::string target;
	bool disabled;
};

void from_json(const json &j, Move &m) {
	j.at("move").get_to(m.move);
	m.id = j.value("id", "");
	m.maxpp = j.value("maxpp", 16);
	m.target = j.value("target", "normal");
	m.disabled = j.value("disabled", false);
}

void PBattle::handle_chunk(const char *chunk) {
	String chunk_ = chunk;
	action_queue_.append_array(chunk_.split("\n"));
}

struct ParsedBattleLine {
	PackedStringArray args;
	Dictionary kw_args;
};

ParsedBattleLine parse_battle_line(const String &line) {
	ParsedBattleLine result;

	// NOTA: Protocol.parseLine(line, true) del original hace un parseo especial
	// para ciertos tipos de línea (chat, raw html, etc). Si tienes esa lógica
	// en otro lado, iría aquí antes del fallback. La omito porque no fue provista.

	if (line == "|") {
		result.args.push_back("done");
		return result;
	}

	if (line.is_empty() || line[0] != '|') {
		// línea inválida según el formato esperado por Showdown
		return result;
	}

	// args = line.slice(1).split('|')
	String rest = line.substr(1);
	PackedStringArray args = rest.split("|");

	// Extraer kwArgs desde el final: mientras el último elemento
	// tenga forma "[clave]valor"
	while (args.size() > 1) {
		String last_arg = args[args.size() - 1];

		if (last_arg.is_empty() || last_arg[0] != '[') {
			break;
		}

		int bracket_pos = last_arg.find("]");
		if (bracket_pos <= 0) {
			break;
		}

		String key = last_arg.substr(1, bracket_pos - 1);
		String value_str = last_arg.substr(bracket_pos + 1).strip_edges();

		if (value_str.is_empty()) {
			result.kw_args[key] = true; // Variant booleano, igual que en el original
		} else {
			result.kw_args[key] = value_str;
		}

		args.remove_at(args.size() - 1);
	}

	result.args = args;
	return result;
}

void PBattle::handle_action(String action) {
	print_line(action);

	auto [args, kw_args] = parse_battle_line(action);
	print_line(args);
	print_line(kw_args);

	ERR_FAIL_COND_MSG(args.size() < 1, "La longitud de args debe ser de al menos 1");

	if (args[0] == "request") {
		json request_payload = json::parse(action.substr(9).utf8().get_data());
		json active = request_payload["active"];

		if (active.is_null())
			return;
		std::vector<Move> moves = active[0]["moves"].get<std::vector<Move>>();

		Array moveset;
		for (auto move : moves) {
			moveset.push_back(String(move.move.c_str()));
		}

		battle_scene_->call("display_moveset", moveset);
	} else if (args[0] == "-damage") {
		ERR_FAIL_COND_MSG(args.size() < 3, "Los args del action -damage deben tener al menos una longitud de 3");

		String ident = args[1]; // "p1a: Bulbasaur"
		String position = ident.substr(0, 3);

		if (args[2].ends_with("fnt")) {
			animation("take_damage", { position, 0 });
			return;
		}

		auto hp_diff = args[2].split("/");

		int hp_max = hp_diff[1].to_int();
		int hp = hp_diff[0].to_int();
		animation("take_damage", { position, hp });

		if (kw_args.has("from")) {
			String from = kw_args["from"];
			if (from == "Recoil") {
				animation("message", { vformat("%s has taken recoil damage", ident.substr(5)) });
			}
		}
	} else if (args[0] == "-heal") {
		ERR_FAIL_COND_MSG(args.size() < 3, "Los args del action -heal deben tener al menos una longitud de 3");

		String ident = args[1];
		String position = ident.substr(0, 3);

		auto hp_diff = args[2].split("/");
		int hp = hp_diff[0].to_int();
		animation("take_damage", { position, hp });

		if (kw_args.has("from")) {
			String from = kw_args["from"];
			// ej: "item: Leftovers" → extraer el nombre del objeto
			if (from.begins_with("item:")) {
				String item_name = from.substr(5).strip_edges();
				animation("message", { vformat("%s restored HP with its %s!", ident.substr(5), item_name) });
			}
		}
	} else if (args[0] == "switch") { // |switch|p1a: Arbok|Arbok, L78, M|254/254
		ERR_FAIL_COND_MSG(args.size() < 4, "Los args del actions switch deben tener al menos una longitud de 4");

		PackedStringArray ident_pokemon = args[1].split(": "); // p1a: Growlithe
		ERR_FAIL_COND_MSG(ident_pokemon.size() < 2, "ident_pokemon debe contener 2 elementos");

		if (ident_pokemon[0] == "p1a") {
			// TODO: se debe incluir una funcion que formate los nombres a id validos
			String pokemon_id = ident_pokemon[1]
										.to_lower()
										.replace("-", "")
										.replace(" ", "");
			String sprite_path = String("res://grafics/pokemon/gen3_back/" + pokemon_id + ".png");

			print_line(sprite_path);

			Sprite2D *sprite = Object::cast_to<Sprite2D>(battle_scene_->get("player_pokemon"));
			Ref<Texture2D> texture = ResourceLoader::get_singleton()->load(sprite_path);

			sprite->set_texture(texture);

			Label *label = Object::cast_to<Label>(battle_scene_->get("player_label"));
			label->set_text(pokemon_id);

			ProgressBar *hp_bar = Object::cast_to<ProgressBar>(battle_scene_->get("player_hp_bar"));

			PackedStringArray hp_diff = args[3].split("/");
			hp_bar->set_max(hp_diff[1].to_int());
			hp_bar->set_value(hp_diff[0].to_int());
		} else if (ident_pokemon[0] == "p2a") {
			String pokemon_id = ident_pokemon[1].to_lower().replace("-", "");
			String sprite_path = String("res://grafics/pokemon/gen3/" + pokemon_id + ".png");

			print_line(sprite_path);

			Sprite2D *sprite = Object::cast_to<Sprite2D>(battle_scene_->get("foe_pokemon"));
			Ref<Texture2D> texture = ResourceLoader::get_singleton()->load(sprite_path);

			sprite->set_texture(texture);

			Label *label = Object::cast_to<Label>(battle_scene_->get("foe_label"));
			label->set_text(pokemon_id);

			ProgressBar *hp_bar = Object::cast_to<ProgressBar>(battle_scene_->get("foe_hp_bar"));
			PackedStringArray hp_diff = args[3].split("/");
			hp_bar->set_max(hp_diff[1].to_int());
			hp_bar->set_value(hp_diff[0].to_int());
		}
	} else if (args[0] == "win") {
		animation("message", { vformat("%s win!\n", args[1]) });
	} else if (args[0] == "faint") {
		String position = args[1].substr(0, 3);
		animation("take_damage", { position, 0 });
		animation("message", { vformat("%s has fainted!\n", args[1].substr(5)) });
	} else if (args[0] == "error") {
		if (kw_args.has("Invalid choice")) {
			battle_scene_->call("show_options");
		}
	} else if (args[0] == "move") {
		String ident = args[1];
		String move = args[2];

		animation("message", { vformat("%s has used \n%s", ident, move) });
	} else if (args[0] == "-ability") {
		String ident = args[1];
		String pokemon = ident.substr(5);
		String ability = args[2];

		animation("splash", { pokemon, ability });
	} else if (args[0] == "-resisted") {
		animation("message", { String("It's not very effective...") });
	} else if (args[0] == "-supereffective") {
		animation("message", { String("It's very effective...") });
	} else if (args[0] == "-unboost") {
		ERR_FAIL_COND_MSG(args.size() < 4, "Los args del action -unboost deben tener al menos una longitud de 4");

		String pokemon = args[1].substr(5); // quitar "p1a: "
		String stat = args[2];
		String stages = args[3];

		animation("message", { vformat("%s's %s fell!", pokemon, stat) });
	} else if (args[0] == "-boost") {
		ERR_FAIL_COND_MSG(args.size() < 4, "Los args del action -boost deben tener al menos una longitud de 4");

		String pokemon = args[1].substr(5); // quitar "p1a: "
		String stat = args[2];
		String stages = args[3];

		animation("message", { vformat("%s's %s rose!", pokemon, stat) });
	} else if (args[0] == "-activate") {
		ERR_FAIL_COND_MSG(args.size() < 3, "Los args del action -activate deben tener al menos una longitud de 3");

		String pokemon = args[1].substr(5);
		String effect = args[2]; // "move: Protect"

		if (effect.begins_with("move:")) {
			String move_name = effect.substr(5).strip_edges();
			animation("message", { vformat("%s protected itself!", pokemon) });
		}
	} else if (args[0] == "-fail") {
		ERR_FAIL_COND_MSG(args.size() < 2, "Los args del action -fail deben tener al menos una longitud de 2");

		String pokemon = args[1].substr(5);

		if (kw_args.has("from")) {
			String from = kw_args["from"];
			// ej: "ability: Own Tempo"
			if (from.begins_with("ability:")) {
				String ability_name = from.substr(8).strip_edges();
				animation("message", { vformat("%s's %s prevents that!", pokemon, ability_name) });
			}
		} else {
			animation("message", { vformat("But it failed for %s!", pokemon) });
		}
	} else if (args[0] == "poke" || args[0] == "teampreview" || args[0] == "teamsize" || args[0] == "t:" || args[0] == "upkeep" || args[0] == "done") {
		// Acciones de metadata/setup que no requieren animación
		return;
	}
}

void PBattle::write(String fragment) {
	JSValue global_obj = JS_GetGlobalObject(context_);
	JSValue func = JS_GetPropertyStr(context_, global_obj, "write");

	if (JS_IsFunction(context_, func)) {
		JSValue args[1];
		args[0] = JS_NewString(context_, fragment.utf8().get_data());

		JSValue result = JS_Call(context_, func, JS_UNDEFINED, 1, args);

		if (JS_IsException(result))
			print_exception(context_);

		JS_FreeValue(context_, result);
		JS_FreeValue(context_, args[0]);
	} else {
		print_error("write not found");
	}

	JS_FreeValue(context_, func);
	JS_FreeValue(context_, global_obj);

	battle_scene_->call("hide_options");
}

void PBattle::animation(String func, Array argv) {
	Dictionary anim;
	anim["animation"] = func;
	anim["argv"] = argv;
	animation_queue_.push_back(anim);
	// battle_scene_->call("animate", func, argv);
}

void PBattle::animate() {
	Dictionary anim = animation_queue_[0];
	battle_scene_->call("animate", anim["animation"], anim["argv"]);
}

void PBattle::completed(Variant vr) {
	animation_queue_.pop_front();
	if (!animation_queue_.is_empty()) {
		animate();
	}
	// print_line("se ha finalizado el GDScriptFunctionState");
}

void PBattle::_ready() {
	// Verificar la implementacion de async en godot-cpp
	// Variant val = battle_scene_->call("async_func");
	// if (val.get_type() == Variant::OBJECT) {
	// 	Object *obj = val;

	// 	if (obj->get_class() == "GDScriptFunctionState") {
	// 		obj->connect("completed", Callable(this, "completed"));
	// 	}
	// }

	// ───────────────────────────────────────────────
	//        Configuración del runtime de JS
	// ───────────────────────────────────────────────

	runtime_ = JS_NewRuntime();
	context_ = JS_NewContext(runtime_);

	JS_SetContextOpaque(context_, this);
	install_console(context_);

	std::string source = read_file("res://bin/index.js");

	ERR_FAIL_COND_MSG(source.empty(), "No se pudo leer el script: res://bin/index.js");

	JSValue res = JS_Eval(context_, source.c_str(), source.length(), "<SOURCE>", JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(res))
		print_exception(context_);
	JS_FreeValue(context_, res);

	// ───────────────────────────────────────────────
	//    Configuración del escenario de combate
	// ───────────────────────────────────────────────
	battle_scene_ = get_parent();

	battle_scene_->connect("animation_completed", Callable(this, "completed"));

	Node *moveset_container = Object::cast_to<Node>(battle_scene_->get("moveset_container"));
	auto btns = moveset_container->get_children();
	//TODO los botones que no poseen un movimiento asignado deberian estar oculto
	//TODO asignar el tag de el movimiento en este loop
	for (int i = 0; i < btns.size(); i++) {
		Button *btn = Object::cast_to<Button>(btns[i]);
		std::string choice = std::string("move ") + std::to_string(i + 1);
		btn->connect("pressed", Callable(this, "write").bind(choice.c_str()));
	}
}

void PBattle::_process(double delta) {
	if (runtime_ == nullptr || context_ == nullptr) {
		return;
	}

	// Evaluando las entradas del jugador
	int error = JS_ExecutePendingJob(runtime_, &job_context_);

	if (error < 0) {
		print_exception(job_context_);
		set_process_mode(PROCESS_MODE_DISABLED);
		return;
	}

	// Evaluado la salida showdown
	if (animation_queue_.is_empty()) {
		if (action_queue_.size() == 0) {
			// command
			return;
		}
		String action = action_queue_[0];
		ERR_FAIL_COND_MSG(action.is_empty(), "Los action no deben ser cadenas vacías");
		action_queue_.remove_at(0);

		handle_action(action);
		if (!animation_queue_.is_empty()) {
			animate();
		}
	}
}

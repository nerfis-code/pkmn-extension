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

void PBattle::_bind_methods() {
	BIND_ENUM_CONSTANT(STATE_CONNECTING);
	BIND_ENUM_CONSTANT(STATE_OPEN);
	BIND_ENUM_CONSTANT(STATE_CLOSED);

	ClassDB::bind_method(D_METHOD("initialize", "scene", "player_name", "packed_team"), &PBattle::initialize);
	ClassDB::bind_method(D_METHOD("prepare", "player_name", "packed_team"), &PBattle::prepare);
	ClassDB::bind_method(D_METHOD("start"), &PBattle::start);
	ClassDB::bind_method(D_METHOD("poll"), &PBattle::poll);
	ClassDB::bind_method(D_METHOD("get_ready_state"), &PBattle::get_ready_state);
	ClassDB::bind_method(D_METHOD("_stdin", "input"), &PBattle::_stdin);
	ClassDB::bind_method(D_METHOD("send", "packet"), &PBattle::send);
}

PBattle::PBattle() {
	runtime_ = JS_NewRuntime();
	context_ = JS_NewContext(runtime_);
}

void PBattle::initialize(Node *scene, String player_name, String packed_team) {
	scene_ = scene;

	if (context_ != nullptr) {
		JS_SetContextOpaque(context_, this);
	}

	thread_ = memnew(Thread);
	thread_->start(Callable(this, "prepare").bind(player_name, packed_team));
}

PBattle::~PBattle() {
	if (thread_.is_valid()) {
		thread_->wait_to_finish();
	}

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

static JSValue js_anim(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	if (argc < 1) {
		print_error("anim() requires one argument");
		return JS_UNDEFINED;
	}

	auto animation = JS_ToCString(ctx, argv[0]);
	if (animation == nullptr) {
		print_error("anim() expected a string");
		return JS_UNDEFINED;
	}

	PBattle *battle = static_cast<PBattle *>(JS_GetContextOpaque(ctx));
	if (battle == nullptr) {
		print_error("JS context has no Godot owner");
		JS_FreeCString(ctx, animation);
		return JS_UNDEFINED;
	}

	Node *scene = Object::cast_to<Node>(battle->scene_);
	if (scene == nullptr) {
		print_error("No scene was assigned to PClient");
		JS_FreeCString(ctx, animation);
		return JS_UNDEFINED;
	}

	scene->call(godot::String(animation));
	JS_FreeCString(ctx, animation);

	return JS_UNDEFINED;
}

void install_scene(JSContext *ctx) {
	JSValue globalthis = JS_GetGlobalObject(ctx);
	JSValue scene = JS_NewObject(ctx);
	JSValue anim = JS_NewCFunction(ctx, &js_anim, "anim", 1);

	JS_SetPropertyStr(ctx, scene, "anim", anim);
	JS_SetPropertyStr(ctx, globalthis, "scene", scene);
	JS_FreeValue(ctx, globalthis);
}

void PBattle::prepare(godot::String player_name, godot::String packed_team) {
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
	install_scene(context_);

	js_std_eval_binary(context_, qjsc_out, qjsc_out_size, 0);

	print_line(qjsc_out_size);
	ready_state_ = STATE_OPEN;
}

void PBattle::start() {
	ready_state_ = STATE_OPEN;
}

void PBattle::_stdin(String in) {
	print_line(in);
}

void PBattle::poll() {
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

int PBattle::get_ready_state() {
	return ready_state_;
}

void PBattle::send(String packet) {
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

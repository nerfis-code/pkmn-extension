#include "pclient.h"
#include "jsrutime.h"

#include <nlohmann/json.hpp>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/variant.hpp>

extern "C" {
#include "quickjs.h"
}

using json = nlohmann::json;
using namespace godot;

void PClient::_bind_methods() {
  ClassDB::bind_method(D_METHOD("_stdin", "in"), &PClient::_stdin);
  ClassDB::bind_method(D_METHOD("handler", "scene", "chuck"), &PClient::handler);
}

PClient::PClient() {}

PClient::~PClient() {
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

void PClient::_stdin(godot::String in) {
  print_line(in);
}

void PClient::_ready() {
	// ───────────────────────────────────────────────
	//        Configuración del runtime de JS
	// ───────────────────────────────────────────────

	runtime_ = JS_NewRuntime();
	if (runtime_ == nullptr) {
		print_error("Failed to create JS runtime");
		return;
	}

	context_ = JS_NewContext(runtime_);
	if (context_ == nullptr) {
		print_error("Failed to create JS context");
		return;
	}

	JS_SetContextOpaque(context_, this);
	install_console(context_);

	std::string source = read_file("res://bin/client.js");
	if (source.empty()) {
		print_error("Could not load JS script from res://bin/client.js");
		return;
	}

	JSValue res = JS_Eval(context_, source.c_str(), source.length(), "<SOURCE>", JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(res)) {
		print_exception(context_);
	} else {
		print_line("JS initialized");
	}
	JS_FreeValue(context_, res);
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

	PClient *client = static_cast<PClient *>(JS_GetContextOpaque(ctx));
	if (client == nullptr) {
		print_error("JS context has no Godot owner");
		JS_FreeCString(ctx, animation);
		return JS_UNDEFINED;
	}

	Node *scene = Object::cast_to<Node>(client->scene_);
	if (scene == nullptr) {
		print_error("No scene was assigned to PClient");
		JS_FreeCString(ctx, animation);
		return JS_UNDEFINED;
	}

	scene->call(godot::String(animation));
	JS_FreeCString(ctx, animation);

	return JS_UNDEFINED;
}

JSValue create_scene(JSContext *ctx) {
	JSValue scene = JS_NewObject(ctx);
	JSValue anim = JS_NewCFunction(ctx, &js_anim, "anim", 1);

	JS_SetPropertyStr(ctx, scene, "anim", anim);
  return scene;
}

bool PClient::handler(Node *scene, String chuck) {
	if (context_ == nullptr) {
		print_error("JS context is not initialized");
		return false;
	}

	if (scene == nullptr) {
		print_error("Scene argument is null");
		return false;
	}

	JSValue global_obj = JS_GetGlobalObject(context_);
	JSValue func = JS_GetPropertyStr(context_, global_obj, "handler");
	scene_ = scene;

	JSValue cc = create_scene(context_);
	bool ok = false;

	if (JS_IsFunction(context_, func)) {
		JSValue args[2];
		args[0] = cc;
		args[1] = JS_NewString(context_, chuck.utf8().get_data());

		JSValue result = JS_Call(context_, func, JS_UNDEFINED, 2, args);
		if (JS_IsException(result)) {
			print_exception(context_);
		} else {
			ok = true;
		}

		JS_FreeValue(context_, result);
		JS_FreeValue(context_, args[0]);
		JS_FreeValue(context_, args[1]);
	} else {
		print_error("handler not found");
	}

	JS_FreeValue(context_, func);
	JS_FreeValue(context_, global_obj);

	return ok;
}


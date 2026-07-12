#include "pserver.h"
#include "jsrutime.h"

#include <nlohmann/json.hpp>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/variant.hpp>

extern "C" {
#include "quickjs.h"
}

using json = nlohmann::json;
using namespace godot;

void PServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_species", "species"), &PServer::get_species);
  ClassDB::bind_method(D_METHOD("_stdin", "in"), &PServer::_stdin);
}

PServer::PServer() {}

PServer::~PServer() {
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

void PServer::_stdin(godot::String in) {
  print_line(in);
}

void PServer::_ready() {
	// ───────────────────────────────────────────────
	//        Configuración del runtime de JS
	// ───────────────────────────────────────────────

	runtime_ = JS_NewRuntime();
	context_ = JS_NewContext(runtime_);

	JS_SetContextOpaque(context_, this);
	install_console(context_);

	std::string source = read_file("res://bin/pokedex.js");

	JSValue res = JS_Eval(context_, source.c_str(), source.length(), "<SOURCE>", JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(res)) print_exception(context_);
	JS_FreeValue(context_, res);
}

String PServer::get_species(String species) const {
	JSValue global_obj = JS_GetGlobalObject(context_);
	JSValue func = JS_GetPropertyStr(context_, global_obj, "getSpecies");
	String species_data;

	if (JS_IsFunction(context_, func)) {
		JSValue args[1];
		args[0] = JS_NewString(context_, species.utf8().get_data());

		JSValue result = JS_Call(context_, func, JS_UNDEFINED, 1, args);

		if (JS_IsException(result))
			print_exception(context_);

		species_data = JS_ToCString(context_, result);

		JS_FreeValue(context_, result);
		JS_FreeValue(context_, args[0]);
	} else {
		print_error("getSpecies not found");
		return "";
	}

	JS_FreeValue(context_, func);
	JS_FreeValue(context_, global_obj);

	return species_data;
}
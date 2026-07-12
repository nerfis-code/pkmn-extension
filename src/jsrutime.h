#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/classes/node.hpp>

extern "C" {
#include "quickjs.h"
}

inline JSValue js_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	godot::Node *sd = static_cast<godot::Node *>(JS_GetContextOpaque(ctx));

	for (int i = 0; i < argc; i++) {
		const char *output = JS_ToCString(ctx, argv[i]);

		if (output == nullptr) {
			continue;
		}
		sd->call("_stdin", godot::String(output));

		JS_FreeCString(ctx, output);
	}
	return JS_UNDEFINED;
}

inline std::string read_file(const char *path) {
  if (!godot::FileAccess::file_exists(path)) {
    return "";
  }
	godot::String ssource = godot::FileAccess::open(godot::String(path), godot::FileAccess::READ)->get_as_text();
	std::string source = ssource.utf8().get_data();

	return source;
}

inline void install_console(JSContext *ctx) {
	JSValue globalthis = JS_GetGlobalObject(ctx);
	JSValue console = JS_NewObject(ctx);
	JSValue log = JS_NewCFunction(ctx, &js_console_log, "log", 1);

	JS_SetPropertyStr(ctx, console, "log", log);
	JS_SetPropertyStr(ctx, globalthis, "console", console);
	JS_FreeValue(ctx, globalthis);
}

inline void print_exception(JSContext *ctx) {
	JSValue exception = JS_GetException(ctx);
	const char *error_str = JS_ToCString(ctx, exception);
	if (error_str != nullptr) {
		godot::print_error(error_str);
		JS_FreeCString(ctx, error_str);
	}
	JS_FreeValue(ctx, exception);
}

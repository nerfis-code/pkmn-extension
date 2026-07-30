#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_data_container.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <string>
#include <string_view>

extern "C" {
#include "quickjs.h"
}

class PClient : public godot::Node {
	GDCLASS(PClient, godot::Node)
public:
	PClient();
	~PClient();

	void _ready() override;
	void _stdin(godot::String in);
  bool handler(godot::Node *scene, godot::String chuck);
  godot::Node *scene_;
protected:
	static void _bind_methods();

private:
	JSRuntime *runtime_ = nullptr;
	JSContext *context_ = nullptr;
	JSContext *job_context_ = nullptr;
};
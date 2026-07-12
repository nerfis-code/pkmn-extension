#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_data_container.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <string>
#include <string_view>

extern "C" {
#include "quickjs.h"
}

class PServer : public godot::Node {
	GDCLASS(PServer, godot::Node)
public:
	PServer();
	~PServer();

  void _ready() override;
  godot::String get_species(godot::String species) const;
  void _stdin(godot::String in);
protected:
	static void _bind_methods();
    
private:
	JSRuntime *runtime_ = nullptr;
	JSContext *context_ = nullptr;
	JSContext *job_context_ = nullptr;
};
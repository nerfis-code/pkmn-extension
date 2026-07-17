#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_data_container.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <string>
#include <string_view>

extern "C" {
#include "quickjs.h"
}

class PBattle : public godot::Node {
	GDCLASS(PBattle, godot::Node)
public:
	PBattle();
	~PBattle();

	void completed(godot::Variant vr);
	void _ready() override;
	void _process(double delta) override;

  void choose(godot::String input);
  void _stdin(godot::String in);
	void start(godot::String player_name, godot::String packed_team);
protected:
	static void _bind_methods();
    
private:
	void handle_action(godot::String action);
	void animation(godot::String func, godot::Array argv);
	void animate();

	godot::Node *battle_scene_ = nullptr;
	JSRuntime *runtime_ = nullptr;
	JSContext *context_ = nullptr;
	JSContext *job_context_ = nullptr;
	bool active_request_ = false;
	godot::PackedStringArray action_queue_{};
	godot::TypedArray<godot::Dictionary> animation_queue_{};
};

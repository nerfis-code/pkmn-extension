#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_data_container.hpp>
#include <godot_cpp/classes/thread.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <string>
#include <string_view>

extern "C" {
#include "quickjs.h"
}

class PBattle : public godot::RefCounted {
	GDCLASS(PBattle, godot::RefCounted)

public:
	PBattle();
	~PBattle();

	void initialize(godot::Node *scene, godot::String player_name, godot::String packed_team);

	enum State {
		STATE_CONNECTING,
		STATE_OPEN,
		STATE_CLOSED
	};

	void start();
	void poll();
	int get_ready_state();
	void send(godot::String packet);
	void prepare(godot::String player_name, godot::String packed_team);
	void _stdin(godot::String in);
	godot::Node *scene_ = nullptr;
	
protected:
	static void _bind_methods();

private:
	State ready_state_ = STATE_CONNECTING;
	JSRuntime *runtime_ = nullptr;
	JSContext *context_ = nullptr;
	JSContext *job_context_ = nullptr;
	godot::Ref<godot::Thread> thread_ = nullptr;
};

VARIANT_ENUM_CAST(PBattle::State);
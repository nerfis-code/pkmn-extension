#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/packed_data_container.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <string>
#include <string_view>

extern "C" {
#include "quickjs.h"
}

class PBattlePeer : public godot::RefCounted {
	GDCLASS(PBattlePeer, godot::RefCounted)

public:
	PBattlePeer();
	~PBattlePeer();

	enum State {
		STATE_CONNECTING,
		STATE_OPEN,
		STATE_CLOSED
	};

	void prepare(godot::String player_name, godot::String packed_team);
	void start();
	void poll();
	int get_ready_state();
	int get_available_packet_count();
	godot::String get_packet();
	void send(godot::String packet);
	void _stdin(godot::String in);

protected:
	static void _bind_methods();

private:
	State ready_state_ = STATE_CONNECTING;
	godot::TypedArray<godot::String> packet_queue_{};
	JSRuntime *runtime_ = nullptr;
	JSContext *context_ = nullptr;
	JSContext *job_context_ = nullptr;
};

VARIANT_ENUM_CAST(PBattlePeer::State);
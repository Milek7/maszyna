#pragma once
#include "network/message.h"
#include "command.h"
#include <queue>

namespace network
{
    struct message
	{
		enum type_e
		{
			CONNECT_REQUEST = 0,
			CONNECT_ACCEPT,
			STEP_INFO,
			VEHICLE_COMMAND,
			TYPE_MAX
		};

		type_e type;

		message(type_e t) : type(t) {}
		virtual void serialize(std::ostream &stream);
		virtual void deserialize(std::istream &stream);
		virtual size_t get_size() { return 2; }
	};

	struct delta_message : public message
	{
		delta_message() : message(STEP_INFO) {}

		double dt;
		command_queue::commanddatasequence_map commands;

		virtual void serialize(std::ostream &stream) override;
		virtual void deserialize(std::istream &stream) override;
		virtual size_t get_size() override;
	};

	std::shared_ptr<message> deserialize_message(std::istream &stream);
}
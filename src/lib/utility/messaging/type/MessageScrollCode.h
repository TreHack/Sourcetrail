#ifndef MESSAGE_SCROLL_CODE_H
#define MESSAGE_SCROLL_CODE_H

#include "utility/messaging/Message.h"

class MessageScrollCode
	: public Message<MessageScrollCode>
{
public:
	MessageScrollCode(int value, bool inListMode)
		: value(value)
		, inListMode(inListMode)
	{
		setIsLogged(false);
	}

	static const std::string getStaticType()
	{
		return "MessageScrollCode";
	}

	int value;
	bool inListMode;
};

#endif // MESSAGE_SCROLL_CODE_H

#pragma once

#include <Windows.h>

namespace pingstats // export
{
	enum WindowMessage
	{
		WM_NOTIFICATIONICON = 1 + WM_APP, 
		WM_REDRAW, 
		WM_PING_RESULT, 
		WM_TRACE_RESULT, 
	};
}

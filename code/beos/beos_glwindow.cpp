#include "beos_glwindow.h"
#include <malloc.h>
#include "../client/client.h"
#include "beos_local.h"
#include "../renderer/tr_local.h"


#define	WINDOW_NAME	"Quake III: Arena"

QWindow::QWindow(BRect r, bool fullscreen) : BDirectWindow(r, WINDOW_NAME, B_TITLED_WINDOW, B_NOT_RESIZABLE|B_NOT_ZOOMABLE|B_NOT_CLOSABLE|B_ASYNCHRONOUS_CONTROLS) {
	fConnected = false;
	fConnectionDisabled = false;
	fClipList = NULL;
	fNumClipRects = 0;
	fFullscreen = fullscreen;

	//if (fullscreen)
	//	SetFullScreen(true);

	fView = new QGLView(Bounds(), "q3view", 0, 0, BGL_RGB | BGL_DEPTH | BGL_DOUBLE);
	AddChild(fView);
	fView->EnableDirectMode(true);
	fView->MakeFocus(true);
	Show();
};

QWindow::~QWindow(){
	fView->UnlockGL();
	fConnectionDisabled = true;
	Hide();
	RemoveChild(fView);
	delete fView;
	Sync();
	free(fClipList);
};

bool QWindow::QuitRequested() {
	//printf("JENS------------GLWindow::~QuitRequested()\n");
	return true;
};

void QWindow::DirectConnected(direct_buffer_info *info) {
	if(fView)
		fView->DirectConnected(info);	
	fView->EnableDirectMode(true);
};

void QGLView::KeyDown(const char* bytes, int32 numbytes) {
	if (numbytes == 1) {
		switch(bytes[0]) {
			case B_DOWN_ARROW:
				Sys_QueEvent(0, SE_KEY, K_DOWNARROW, qtrue, 0, NULL); break;
			case B_UP_ARROW:
				Sys_QueEvent(0, SE_KEY, K_UPARROW, qtrue, 0, NULL);	break;
			case B_LEFT_ARROW:
				Sys_QueEvent(0, SE_KEY, K_LEFTARROW, qtrue, 0, NULL); break;
			case B_RIGHT_ARROW:
				Sys_QueEvent(0, SE_KEY, K_RIGHTARROW, qtrue, 0, NULL);break;
			case B_ESCAPE:
				Sys_QueEvent(0, SE_KEY, K_ESCAPE, qtrue, 0, NULL);break;
			case B_RETURN:
				Sys_QueEvent(0, SE_KEY, K_ENTER, qtrue, 0, NULL);break;
			case B_BACKSPACE:
				Sys_QueEvent(0, SE_KEY, K_BACKSPACE, qtrue, 0, NULL);break;
			case B_SPACE:
				Sys_QueEvent(0, SE_KEY, K_SPACE, qtrue, 0, NULL);break;
			case B_TAB:
				Sys_QueEvent(0, SE_KEY, K_TAB, qtrue, 0, NULL);	break;
			case B_INSERT:
				Sys_QueEvent(0, SE_KEY, K_INS, qtrue, 0, NULL);	break;
			case B_DELETE:
				Sys_QueEvent(0, SE_KEY, K_DEL, qtrue, 0, NULL);	break;
			case B_HOME:
				Sys_QueEvent(0, SE_KEY, K_HOME, qtrue, 0, NULL);break;
			case B_END:
				Sys_QueEvent(0, SE_KEY, K_END, qtrue, 0, NULL);	break;
			case B_PAGE_UP:
				Sys_QueEvent(0, SE_KEY, K_PGUP, qtrue, 0, NULL);break;
			case B_PAGE_DOWN:
				Sys_QueEvent(0, SE_KEY, K_PGDN, qtrue, 0, NULL);break;
			default:
				Sys_QueEvent(0, SE_KEY, tolower(bytes[0]), qtrue, 0, NULL);
		}
	}
}
void QGLView::KeyUp(const char* bytes, int32 numbytes) {
	if (numbytes == 1) {
		switch(bytes[0]) {
		//Todo add list from InterfaceDefs.h, put in order
			case B_DOWN_ARROW:
				Sys_QueEvent(0, SE_KEY, K_DOWNARROW, qfalse, 0, NULL); break;
			case B_UP_ARROW:
				Sys_QueEvent(0, SE_KEY, K_UPARROW, qfalse, 0, NULL);break;
			case B_LEFT_ARROW:
				Sys_QueEvent(0, SE_KEY, K_LEFTARROW, qfalse, 0, NULL); break;
			case B_RIGHT_ARROW:
				Sys_QueEvent(0, SE_KEY, K_RIGHTARROW, qfalse, 0, NULL);break;
			case B_ESCAPE:
				Sys_QueEvent(0, SE_KEY, K_ESCAPE, qfalse, 0, NULL);break;
			case B_RETURN:
				Sys_QueEvent(0, SE_KEY, K_ENTER, qfalse, 0, NULL);break;
			case B_BACKSPACE:
				Sys_QueEvent(0, SE_KEY, K_BACKSPACE, qfalse, 0, NULL);break;
			case B_SPACE:
				Sys_QueEvent(0, SE_KEY, K_SPACE, qfalse, 0, NULL);break;
			case B_TAB:
				Sys_QueEvent(0, SE_KEY, K_TAB, qfalse, 0, NULL);break;
			case B_INSERT:
				Sys_QueEvent(0, SE_KEY, K_INS, qfalse, 0, NULL);break;
			case B_DELETE:
				Sys_QueEvent(0, SE_KEY, K_DEL, qfalse, 0, NULL);break;
			case B_HOME:
				Sys_QueEvent(0, SE_KEY, K_HOME, qfalse, 0, NULL);break;
			case B_END:
				Sys_QueEvent(0, SE_KEY, K_END, qfalse, 0, NULL);break;
			case B_PAGE_UP:
				Sys_QueEvent(0, SE_KEY, K_PGUP, qfalse, 0, NULL);break;
			case B_PAGE_DOWN:
				Sys_QueEvent(0, SE_KEY, K_PGDN, qfalse, 0, NULL);break;
			default:
				Sys_QueEvent(0, SE_KEY, tolower(bytes[0]), qfalse, 0, NULL);
		}	
	}
}

void QGLView::MouseMoved(BPoint point, uint32 transit, const BMessage *message) {
	if (!Window()->IsActive())
		return;

	// If the cursor is already centered on screen, return;
	// Because set_mouse_position below issues a MouseMoved() :/
	if (point.x == glConfig.vidWidth/2 && point.y == glConfig.vidHeight/2)
		return;

	int mx, my = 0;
	mx = (int)point.x - (glConfig.vidWidth/2);
	my = (int)point.y - (glConfig.vidHeight/2);

	Sys_QueEvent(0, SE_MOUSE, mx, my, 0, NULL);

	//Center on screen
	set_mouse_position((long int)(Window()->Frame().right - Window()->Bounds().Width()/2)+1, (long int)(Window()->Frame().bottom - Window()->Bounds().Height()/2)+1);
}

void QGLView::MouseDown(BPoint point) {
	SetMouseEventMask(B_POINTER_EVENTS|B_KEYBOARD_EVENTS);
   	int32 buttons;
   	if (Window()->CurrentMessage()->FindInt32("buttons", (int32 *)&buttons) == B_NO_ERROR) {
		if (buttons & B_PRIMARY_MOUSE_BUTTON)
   		  	Sys_QueEvent(0, SE_KEY, K_MOUSE1, qtrue, 0, NULL);
		if (buttons & B_SECONDARY_MOUSE_BUTTON)
			Sys_QueEvent(0, SE_KEY, K_MOUSE2, qtrue, 0, NULL);
		if (buttons & B_TERTIARY_MOUSE_BUTTON)
			Sys_QueEvent(0, SE_KEY, K_MOUSE3, qtrue, 0, NULL);
		old_buttons = buttons;
   	}
}

void QGLView::MouseUp(BPoint point) {
	uint32 buttons;
	if (Window()->CurrentMessage()->FindInt32("buttons", (int32 *)&buttons) == B_NO_ERROR) {
		if ((buttons ^ B_PRIMARY_MOUSE_BUTTON) & old_buttons) {
			Sys_QueEvent(0, SE_KEY, K_MOUSE1, qfalse, 0, NULL);
		}
		if ((buttons ^ B_SECONDARY_MOUSE_BUTTON) & old_buttons) {
			Sys_QueEvent(0, SE_KEY, K_MOUSE2, qfalse, 0, NULL);
		}
		if ((buttons ^ B_TERTIARY_MOUSE_BUTTON) & old_buttons) {
			Sys_QueEvent(0, SE_KEY, K_MOUSE3, qfalse, 0, NULL);
		}
		old_buttons = buttons;
	}
}

void QGLView::MouseScrolled(float delta_x, float delta_y) {
	if (delta_y < 0) {
		Sys_QueEvent(0, SE_KEY, K_MWHEELUP, qtrue, 0, NULL);
		Sys_QueEvent(0, SE_KEY, K_MWHEELUP, qfalse, 0, NULL);
	} else {
		Sys_QueEvent(0, SE_KEY, K_MWHEELDOWN, qtrue, 0, NULL);
		Sys_QueEvent(0, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL);
	}
}


void QGLView::UnmappedKeyDown(int32 key) {
	switch (key) {
		case 34  : /*B_NUM_LOCK*/
			Sys_QueEvent(0, SE_KEY, K_KP_NUMLOCK, qtrue, 0, NULL); break;
		case 59  : /*B_CAPS_LOCK*/
			Sys_QueEvent(0, SE_KEY, K_CAPSLOCK, qtrue, 0, NULL); break;
		case 75  : /*B_LEFT_SHIFT_KEY*/
			Sys_QueEvent(0, SE_KEY, K_SHIFT, qtrue, 0, NULL); break;
		case 86  : /*B_RIGHT_SHIFT_KEY*/
			Sys_QueEvent(0, SE_KEY, K_SHIFT, qtrue, 0, NULL); break;
		case 92  : /*B_LEFT_CONTROL_KEY*/
			Sys_QueEvent(0, SE_KEY, K_CTRL, qtrue, 0, NULL); break;
		case 93  : /*B_LEFT_COMMAND_KEY*/
			Sys_QueEvent(0, SE_KEY, K_ALT, qtrue, 0, NULL); break;
		case 95  : /*B_RIGHT_COMMAND_KEY*/
			Sys_QueEvent(0, SE_KEY, K_ALT, qtrue, 0, NULL); break;
		case 96  : /*B_RIGHT_CONTROL_KEY*/
			Sys_QueEvent(0, SE_KEY, K_CTRL, qtrue, 0, NULL); break;
		case 102 : /*B_LEFT_OPTION_KEY*/
			Sys_QueEvent(0, SE_KEY, K_COMMAND, qtrue, 0, NULL); break;
		case 103 : /*B_RIGHT_OPTION_KEY*/
			Sys_QueEvent(0, SE_KEY, K_COMMAND, qtrue, 0, NULL); break;
		case 104 : /*B_MENU_KEY*/
			Sys_QueEvent(0, SE_KEY, K_COMMAND, qtrue, 0, NULL); break;
	}
}

void QGLView::UnmappedKeyUp(int32 key) {
	switch (key) {
		case 34  : /*B_NUM_LOCK*/
			Sys_QueEvent(0, SE_KEY, K_KP_NUMLOCK, qfalse, 0, NULL); break;
		case 59  : /*B_CAPS_LOCK*/
			Sys_QueEvent(0, SE_KEY, K_CAPSLOCK, qfalse, 0, NULL); break;
		case 75  : /*B_LEFT_SHIFT_KEY*/
			Sys_QueEvent(0, SE_KEY, K_SHIFT, qfalse, 0, NULL); break;
		case 86  : /*B_RIGHT_SHIFT_KEY*/
			Sys_QueEvent(0, SE_KEY, K_SHIFT, qfalse, 0, NULL); break;
		case 92  : /*B_LEFT_CONTROL_KEY*/
			Sys_QueEvent(0, SE_KEY, K_CTRL, qfalse, 0, NULL); break;
		case 93  : /*B_LEFT_COMMAND_KEY*/
			Sys_QueEvent(0, SE_KEY, K_ALT, qfalse, 0, NULL); break;
		case 95  : /*B_RIGHT_COMMAND_KEY*/
			Sys_QueEvent(0, SE_KEY, K_ALT, qfalse, 0, NULL); break;
		case 96  : /*B_RIGHT_CONTROL_KEY*/
			Sys_QueEvent(0, SE_KEY, K_CTRL, qfalse, 0, NULL); break;
		case 102 : /*B_LEFT_OPTION_KEY*/
			Sys_QueEvent(0, SE_KEY, K_COMMAND, qfalse, 0, NULL); break;
		case 103 : /*B_RIGHT_OPTION_KEY*/
			Sys_QueEvent(0, SE_KEY, K_COMMAND, qfalse, 0, NULL); break;
		case 104 : /*B_MENU_KEY*/
			Sys_QueEvent(0, SE_KEY, K_COMMAND, qfalse, 0, NULL); break;
	}
}

void QGLView::MessageReceived(BMessage *message) {
	switch(message->what) {
		case B_MOUSE_WHEEL_CHANGED :
			float delta_x, delta_y;
			message->FindFloat("be:wheel_delta_x", (float *)&delta_x);
			message->FindFloat("be:wheel_delta_y", (float *)&delta_y);
			MouseScrolled(delta_x, delta_y);
			break;
		case B_UNMAPPED_KEY_DOWN :
			int32 dkey;
			message->FindInt32("key", &dkey);
			UnmappedKeyDown(dkey);
			break;
		case B_UNMAPPED_KEY_UP :
			int32 ukey;
			message->FindInt32("key", &ukey);
			UnmappedKeyUp(ukey);
			break;
		default :
			BGLView::MessageReceived(message);
	}
}
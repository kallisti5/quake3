#ifndef _BEOS_GLWINDOW_H
#define _BEOS_GLWINDOW_H

#include <DirectWindow.h>
#include <GLView.h>

class QGLView : public BGLView {
	public:
		QGLView(BRect frame, const char *name, int32 resizingMode, int32 flags, int32 type):
			BGLView(frame, name, resizingMode, flags, type){
			old_buttons = 0;
		};
		virtual void ErrorCallback(GLenum errorcode) {
			//Com_Printf("GLERROR:%u\n", errorcode);
		};
		virtual void KeyDown(const char* bytes, int32 numbytes);
		virtual void KeyUp(const char* bytes, int32 numbytes);
		virtual void MouseMoved(BPoint point, uint32 transit, const BMessage *message);
		virtual void MouseDown(BPoint point);
		virtual void MouseUp(BPoint point);
		virtual void MouseScrolled(float delta_x, float delta_y);
		virtual void UnmappedKeyDown(int32 key);
		virtual void UnmappedKeyUp(int32 key);
		virtual void MessageReceived(BMessage *message);
		int32 old_buttons;
};

class QWindow : public BDirectWindow {
	public:
		QWindow(BRect r, bool fullscreen);
		~QWindow();
		virtual bool QuitRequested();
		virtual void DirectConnected(direct_buffer_info *info);

		// For drawing
		uint8			*fBits;
		int32			fRowBytes;
		color_space		fFormat;
		clipping_rect	fBounds;
		uint32			fNumClipRects;
		clipping_rect	*fClipList;
		bool			fConnected;
		bool			fConnectionDisabled;

		BGLView			*fView;
		bool			fFullscreen;

};

#endif /* _BEOS_GLWINDOW_H */

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <climits>

#define WANT_ARCAN_SHMIF_HELPER
#include <arcan_shmif.h>

#include "imgui/imgui_impl_opengl3.h"

#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_opengl3_loader.h"

#include "profiler/TracyImGui.hpp"
#include "Backend.hpp"
#include "RunQueue.hpp"
#include "profiler/TracyConfig.hpp"

#include <map>

static struct arcan_shmif_cont C;
static struct arcan_shmif_cont CIcon;
static struct arcan_shmif_cont CClipboard;
static struct arcan_shmif_cont CPaste;

static uint8_t mouse_state[ASHMIF_MSTATE_SZ];

static std::function<void()> s_redraw;
static RunQueue* s_mainThreadTasks;
static float s_scale = 1.0;
static bool s_invisible = false;
static struct {
	uint8_t* data;
	int w, h;
} s_icon;

static std::map<ImGuiMouseCursor, const char*> s_cursorMap;

static void buildCursorLUT()
{
	s_cursorMap[ImGuiMouseCursor_None] = "hidden";
	s_cursorMap[ImGuiMouseCursor_Arrow] = "default";
	s_cursorMap[ImGuiMouseCursor_TextInput] = "typefield";
	s_cursorMap[ImGuiMouseCursor_Hand] = "grabhint";
	s_cursorMap[ImGuiMouseCursor_ResizeNS] = "north-south";
	s_cursorMap[ImGuiMouseCursor_ResizeEW] = "west-east";
	s_cursorMap[ImGuiMouseCursor_ResizeAll] = "sizeall";
	s_cursorMap[ImGuiMouseCursor_NotAllowed] = "forbidden";
}

static struct arcan_event eventFactory(enum ARCAN_EVENT_EXTERNAL kind)
{
	struct arcan_event ret;
	memset(&ret, '\0', sizeof(ret));
	ret.category = EVENT_EXTERNAL;
	ret.ext.kind = kind;
	return ret;
}

static void onClipboardText(void* ud, const char* text)
{
 		struct arcan_event ev = eventFactory(EVENT_EXTERNAL_MESSAGE);
    arcan_shmif_pushutf8(&CClipboard, &ev, text, strlen(text) + 1);
}

static const char* onPasteboardText(void* ud)
{
		static std::string s;
		static bool complete;
    struct arcan_event ev;
		int status;

	  /* ignore other object types (e.g. cut and paste files etc.) */
		while ((status = arcan_shmif_poll(&CPaste, &ev)) > 0){
			if (ev.category == EVENT_TARGET && ev.tgt.kind == TARGET_COMMAND_MESSAGE){
				if (complete){ /* forget the previous one */
					complete = false;
					s = "";
				}

				s = s + static_cast<char*>(ev.tgt.message);
				complete = ev.tgt.ioevs[0].iv == 0;
			}
		}

		if (-1 == status && CPaste.addr){
			arcan_shmif_drop(&CPaste);
		}

		return complete ? s.c_str() : NULL;
}

static void requestSegment(enum ARCAN_SEGID id, int w, int h)
{
		arcan_event segreq = eventFactory(EVENT_EXTERNAL_SEGREQ);
		segreq.ext.segreq.kind = id;
		segreq.ext.segreq.width = w;
		segreq.ext.segreq.height = h;
		arcan_shmif_enqueue(&C, &segreq);
}

static void sendCursor(const char* cursor)
{
   static const char* last_cursor;
	 if (cursor == last_cursor)
		 return;

	 last_cursor = cursor;
   arcan_event ev = eventFactory(EVENT_EXTERNAL_CURSORHINT);
   snprintf((char*)ev.ext.message.data, sizeof(ev.ext.message.data), "%s", cursor);
	 arcan_shmif_enqueue(&C, &ev);
}

/* regular SDL2 sym to imgui key lut */
static ImGuiKey keysymToImGui(int sym)
{
    switch (sym)
    {
        case 9: return ImGuiKey_Tab;
        case 276: return ImGuiKey_LeftArrow;
        case 275: return ImGuiKey_RightArrow;
        case 273: return ImGuiKey_UpArrow;
        case 274: return ImGuiKey_DownArrow;
        case 280: return ImGuiKey_PageUp;
        case 281: return ImGuiKey_PageDown;
        case 278: return ImGuiKey_Home;
        case 279: return ImGuiKey_End;
        case 277: return ImGuiKey_Insert;
        case 127: return ImGuiKey_Delete;
        case 8: return ImGuiKey_Backspace;
        case 32: return ImGuiKey_Space;
        case 13: return ImGuiKey_Enter;
        case 27: return ImGuiKey_Escape;
        case 34: return ImGuiKey_Apostrophe;
        case 44: return ImGuiKey_Comma;
        case 20: return ImGuiKey_Minus;
        case 46: return ImGuiKey_Period;
        case 61: return ImGuiKey_Slash;
        case 59: return ImGuiKey_Semicolon;
        case 21: return ImGuiKey_Equal;
        case 91: return ImGuiKey_LeftBracket;
        case 92: return ImGuiKey_Backslash;
        case 93: return ImGuiKey_RightBracket;
        case 96: return ImGuiKey_GraveAccent;
        case 301: return ImGuiKey_CapsLock;
        case 302: return ImGuiKey_ScrollLock;
        case 300: return ImGuiKey_NumLock;
        case 316: return ImGuiKey_PrintScreen;
        case 19: return ImGuiKey_Pause;
        case 256: return ImGuiKey_Keypad0;
        case 257: return ImGuiKey_Keypad1;
        case 258: return ImGuiKey_Keypad2;
        case 259: return ImGuiKey_Keypad3;
        case 260: return ImGuiKey_Keypad4;
        case 261: return ImGuiKey_Keypad5;
        case 262: return ImGuiKey_Keypad6;
        case 263: return ImGuiKey_Keypad7;
        case 264: return ImGuiKey_Keypad8;
        case 265: return ImGuiKey_Keypad9;
        case 266: return ImGuiKey_KeypadDecimal;
        case 267: return ImGuiKey_KeypadDivide;
        case 268: return ImGuiKey_KeypadMultiply;
        case 269: return ImGuiKey_KeypadSubtract;
        case 270: return ImGuiKey_KeypadAdd;
        case 271: return ImGuiKey_KeypadEnter;
        case 272: return ImGuiKey_KeypadEqual;
        case 306: return ImGuiKey_LeftCtrl;
        case 304: return ImGuiKey_LeftShift;
        case 308: return ImGuiKey_LeftAlt;
        case 310: return ImGuiKey_LeftSuper;
        case 305: return ImGuiKey_RightCtrl;
        case 303: return ImGuiKey_RightShift;
        case 307: return ImGuiKey_RightAlt;
        case 309: return ImGuiKey_RightSuper;
        case 319: return ImGuiKey_Menu;
        case 48: return ImGuiKey_0;
        case 49: return ImGuiKey_1;
        case 50: return ImGuiKey_2;
        case 51: return ImGuiKey_3;
        case 52: return ImGuiKey_4;
        case 53: return ImGuiKey_5;
        case 54: return ImGuiKey_6;
        case 55: return ImGuiKey_7;
        case 56: return ImGuiKey_8;
        case 57: return ImGuiKey_9;
        case 97: return ImGuiKey_A;
        case 98: return ImGuiKey_B;
        case 99: return ImGuiKey_C;
        case 100: return ImGuiKey_D;
        case 101: return ImGuiKey_E;
        case 102: return ImGuiKey_F;
        case 103: return ImGuiKey_G;
        case 104: return ImGuiKey_H;
        case 105: return ImGuiKey_I;
        case 106: return ImGuiKey_J;
        case 107: return ImGuiKey_K;
        case 108: return ImGuiKey_L;
        case 109: return ImGuiKey_M;
        case 110: return ImGuiKey_N;
        case 111: return ImGuiKey_O;
        case 112: return ImGuiKey_P;
        case 113: return ImGuiKey_Q;
        case 114: return ImGuiKey_R;
        case 115: return ImGuiKey_S;
        case 116: return ImGuiKey_T;
        case 117: return ImGuiKey_U;
        case 118: return ImGuiKey_V;
        case 119: return ImGuiKey_W;
        case 120: return ImGuiKey_X;
        case 121: return ImGuiKey_Y;
        case 122: return ImGuiKey_Z;
        case 282: return ImGuiKey_F1;
        case 283: return ImGuiKey_F2;
        case 284: return ImGuiKey_F3;
        case 285: return ImGuiKey_F4;
        case 286: return ImGuiKey_F5;
        case 287: return ImGuiKey_F6;
        case 288: return ImGuiKey_F7;
        case 289: return ImGuiKey_F8;
        case 290: return ImGuiKey_F9;
        case 291: return ImGuiKey_F10;
        case 292: return ImGuiKey_F11;
        case 293: return ImGuiKey_F12;
        case 294: return ImGuiKey_F13;
        case 295: return ImGuiKey_F14;
        case 296: return ImGuiKey_F15;
    }
    return ImGuiKey_None;
}

static void synchIcon()
{
    if (!CIcon.addr)
		    return;

		arcan_shmif_resize(&CIcon, s_icon.w, s_icon.h);
		for (size_t y = 0; y < s_icon.h && y < CIcon.h; y++){
			for (size_t x = 0; x < s_icon.w && x < CIcon.w; x++){
				uint8_t* base = &s_icon.data[(y * s_icon.w + x) * 4];
				CIcon.vidp[y * CIcon.pitch + x] =
					SHMIF_RGBA(base[0], base[1], base[2], base[3]);
			}
		}
		arcan_shmif_signal(&CIcon, SHMIF_SIGVID | SHMIF_SIGBLK_NONE);
}

static bool process_event(struct arcan_event& ev)
{
    bool rv = false;
    if (ev.category == EVENT_IO){
		    ImGuiIO& io = ImGui::GetIO();

		    if (ev.io.devkind == EVENT_IDEVKIND_MOUSE){
				    if (ev.io.datatype == EVENT_IDATATYPE_ANALOG){
						    int x, y;
								if (ev.io.subid == MBTN_WHEEL_UP_IND){
									io.AddMouseWheelEvent(ev.io.input.analog.axisval[0], ev.io.input.analog.axisval[1]);
								}
								else if (arcan_shmif_mousestate(&C, mouse_state, &ev, &x, &y)){
								    io.AddMousePosEvent(x, y);
								}
						}
						else if (ev.io.datatype == EVENT_IDATATYPE_DIGITAL){
							if (ev.io.subid == MBTN_WHEEL_UP_IND){
								io.AddMouseWheelEvent(-8, 0);
							}
							else if (ev.io.subid == MBTN_WHEEL_DOWN_IND){
								io.AddMouseWheelEvent(8, 0);
							}
							else
								io.AddMouseButtonEvent(ev.io.subid - 1, ev.io.input.digital.active);
						}
				}
				else if (ev.io.devkind == EVENT_IDEVKIND_KEYBOARD){
				    if (ev.io.input.translated.utf8[0] && /* printables and full codepoint */
								ev.io.input.translated.utf8[0] > 32 &&
								ev.io.input.translated.utf8[0] != 127){
							io.AddInputCharactersUTF8((char*)ev.io.input.translated.utf8);
						}
						else {
							io.AddKeyEvent(
								keysymToImGui(ev.io.input.translated.keysym), ev.io.input.translated.active);
						}
				}
			  /* There's also touch display and game controllers to consider,
				 * unfortunately we don't have a higher level view of actions to
				 * query labelhints for */
		    return true;
		}

		if (ev.category != EVENT_TARGET)
		    return false;

    switch (ev.tgt.kind){
		case TARGET_COMMAND_STEPFRAME:
		    return true;
		break;
/* for Fonthint we need to special-case building Arcan so that we get
 * to call LoadFontFromCompressed rather than the built-in one and then convert [2].fv from mm to pt */
    case TARGET_COMMAND_FONTHINT:
		break;
		case TARGET_COMMAND_RESET:
			requestSegment(SEGID_ICON, 256, 256);
			requestSegment(SEGID_CLIPBOARD, 0, 0);
		break;
		case TARGET_COMMAND_NEWSEGMENT:
		    if (ev.tgt.ioevs[2].iv == SEGID_CLIPBOARD){
					if (CClipboard.addr)
						arcan_shmif_drop(&CPaste);
					CClipboard = arcan_shmif_acquire(&C, NULL, SEGID_CLIPBOARD, SHMIF_DISABLE_GUARD);
				}
				else if (ev.tgt.ioevs[2].iv == SEGID_ICON){
					if (CIcon.addr)
						arcan_shmif_drop(&CIcon);
					CIcon = arcan_shmif_acquire(&C, NULL, SEGID_ICON, SHMIF_DISABLE_GUARD);
				  synchIcon();
				}
				else if (ev.tgt.ioevs[2].iv == SEGID_CLIPBOARD_PASTE){
					if (CPaste.addr)
						arcan_shmif_drop(&CPaste);
					CPaste = arcan_shmif_acquire(&C, NULL, SEGID_CLIPBOARD_PASTE, SHMIF_DISABLE_GUARD);
				}
		break;
		case TARGET_COMMAND_DISPLAYHINT:
        if (ev.tgt.ioevs[0].iv && ev.tgt.ioevs[1].iv){
				    if (ev.tgt.ioevs[0].iv != C.w || ev.tgt.ioevs[1].iv != C.h){
						    arcan_shmif_resize(&C, ev.tgt.ioevs[0].iv, ev.tgt.ioevs[1].iv);
								rv = true;
								tracy::s_wasActive = true;
						}
				    if (ev.tgt.ioevs[4].fv){
						    s_scale = 38.4 / ev.tgt.ioevs[4].fv;
						}
						if (ev.tgt.ioevs[2].iv & 2){
								s_invisible = true;
						}
						else
								s_invisible = false;
				}
/* ppcm size and focus changes.
 * for resize, just _resize and return true.
 * for focus: AddFocusEvent(true | false)
 * density change is more complicated.
 */
	  break;
		default:
		break;
		}
    return rv;
}

Backend::Backend( const char* title, const std::function<void()>& redraw, const std::function<void(float)>& scaleChanged, const std::function<int(void)>& isBusy, RunQueue* mainThreadTasks )
{
		C = arcan_shmif_open(SEGID_APPLICATION, SHMIF_ACQUIRE_FATALFAIL, NULL);

		/* setup connection and mark as primary so the file-picker parts can reach us */
		struct arcan_shmifext_setup opts = arcan_shmifext_defaults(&C);
    arcan_shmif_mousestate_setup(&C, false, mouse_state);
		arcan_shmif_setprimary(SHMIF_INPUT, &C);

		/* extend to GL3.2 */
		opts.major = 3;
		opts.minor = 2;
		opts.builtin_fbo = 1;
    opts.mask = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
    enum shmifext_setup_status status = arcan_shmifext_setup(&C, opts);
    if (SHMIFEXT_OK != status){
		    fprintf(stderr, "Couldn't extend context to EGL/GL32\n");
				arcan_shmif_last_words(&C, "Accelerated graphics unavailable");
				exit(EXIT_FAILURE);
		}

		/* since it's GL-FBO we have LL origo and we latch on STEPFRAME */
		arcan_shmifext_make_current(&C);
		C.hints |= SHMIF_RHINT_ORIGO_LL | SHMIF_RHINT_VSIGNAL_EV;
		arcan_shmif_resize(&C, C.w, C.h);

		requestSegment(SEGID_ICON, 256, 256);
		requestSegment(SEGID_CLIPBOARD, 32, 32);

		buildCursorLUT();
		s_mainThreadTasks = mainThreadTasks;
		s_redraw = redraw;
 		ImGuiIO& io = ImGui::GetIO();
		io.SetClipboardTextFn = onClipboardText;
		io.GetClipboardTextFn = onPasteboardText;
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
		io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

		ImGui_ImplOpenGL3_Init( "#version 150" );
}

Backend::~Backend()
{
    ImGui_ImplOpenGL3_Shutdown();
    arcan_shmif_drop(&C);
}

void Backend::Show()
{
/* not needed, comes with signal */
}

void Backend::Run()
{
/* poll events, on stepframe do this */
    struct arcan_event ev;
		int pv;

		bool dirty = false;

		while (arcan_shmif_signalstatus(&C) != -1){
		    unsigned ts = 4;
				arcan_shmif_lock(&C); /* lock so that file picker won't race us */
		    if (arcan_shmif_wait_timed(&C, &ts, &ev) > 0){
				    dirty |= process_event(ev);
    		    while ((pv = arcan_shmif_poll(&C, &ev)) > 0){
	    			    dirty |= process_event(ev);
		        }
				}
				arcan_shmif_unlock(&C);
				if (!s_invisible)
					s_redraw();
				s_mainThreadTasks->Run();
		}
}

void Backend::Attention()
{
    arcan_event ev = eventFactory(EVENT_EXTERNAL_ALERT);
		arcan_shmif_enqueue(&C, &ev);
}

void Backend::NewFrame(int &w, int& h)
{
	w = C.w;
	h = C.h;

	/* make sure we draw to the FBO of the context, and clear previous contents */
	arcan_shmifext_bind(&C);
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(C.w, C.h);
	ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
	const char* ctag = s_cursorMap[cursor];
	if (ctag){
		sendCursor(ctag);
	}
	ImGui_ImplOpenGL3_NewFrame();
}

void Backend::EndFrame()
{
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    arcan_shmifext_signal(&C,
		    0, SHMIF_SIGVID | SHMIF_SIGBLK_NONE, SHMIFEXT_BUILTIN);
}

void Backend::SetIcon(uint8_t* data, int w, int h)
{
    s_icon.data = data;
		s_icon.w = w;
		s_icon.h = h;
}

void Backend::SetTitle(const char* title)
{
	arcan_event ev = eventFactory(EVENT_EXTERNAL_IDENT);
	arcan_shmif_pushutf8(&C, &ev, title, strlen(title) + 1);
}

float Backend::GetDpiScale()
{
	return s_scale;
}

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

static struct arcan_shmif_cont C;
static uint8_t mouse_state[ASHMIF_MSTATE_SZ];

static std::function<void()> s_redraw;
static RunQueue* s_mainThreadTasks;

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

Backend::Backend( const char* title, const std::function<void()>& redraw, const std::function<void(float)>& scaleChanged, const std::function<int(void)>& isBusy, RunQueue* mainThreadTasks )
{
/* open, go to ext, set version 3, 2, CORE_PROFILE, FORWARD_COMPAT */
		C = arcan_shmif_open(SEGID_MEDIA, SHMIF_ACQUIRE_FATALFAIL, NULL);

		struct arcan_shmifext_setup opts = arcan_shmifext_defaults(&C);
    arcan_shmif_mousestate_setup(&C, false, mouse_state);
		arcan_shmif_setprimary(SHMIF_INPUT, &C);

		opts.major = 3;
		opts.minor = 2;
		opts.builtin_fbo = 1;
    opts.mask = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT;
    enum shmifext_setup_status status = arcan_shmifext_setup(&C, opts);
    if (SHMIFEXT_OK != status){
		    fprintf(stderr, "Couldn't extend context to EGL/GL32\n");
				exit(EXIT_FAILURE);
		}
		arcan_shmifext_make_current(&C);

		C.hints |= SHMIF_RHINT_ORIGO_LL | SHMIF_RHINT_VSIGNAL_EV;
		arcan_shmif_resize(&C, C.w, C.h);

		s_mainThreadTasks = mainThreadTasks;
		s_redraw = redraw;
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

static bool process_event(struct arcan_event& ev)
{
    if (ev.category == EVENT_IO){
		    ImGuiIO& io = ImGui::GetIO();

		    if (ev.io.devkind == EVENT_IDEVKIND_MOUSE){
				    if (ev.io.datatype == EVENT_IDATATYPE_ANALOG){
						    int x, y;
						    if (arcan_shmif_mousestate(&C, mouse_state, &ev, &x, &y)){
								    io.AddMousePosEvent(x, y);
								}
								else {
									/* subid for wheel should map to:
									 * io.AddMouseWheelEvent */
								}
						}
						else if (ev.io.datatype == EVENT_IDATATYPE_DIGITAL){
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
 * to call LoadFontFromCompressed rather than the built-in one */
    case TARGET_COMMAND_FONTHINT:
		break;
		case TARGET_COMMAND_NEWSEGMENT:
/* for icon and clipboard */
		break;
		case TARGET_COMMAND_DISPLAYHINT:
/* ppcm size and focus changes.
 * for resize, just _resize and return true.
 * for focus: AddFocusEvent(true | false)
 * density change is more complicated.
 */
	  break;
		default:
		break;
		}
    return false;
}

void Backend::Run()
{
/* poll events, on stepframe do this */
    struct arcan_event ev;
		int pv;

		bool dirty = false;

		while (arcan_shmif_signalstatus(&C) != -1){
		    unsigned ts = 16;
				arcan_shmif_lock(&C); /* lock so that file picker won't race us */
		    if (arcan_shmif_wait_timed(&C, &ts, &ev) > 0){
				    dirty |= process_event(ev);
    		    while ((pv = arcan_shmif_poll(&C, &ev)) > 0){
	    			    dirty |= process_event(ev);
		        }
				    if (dirty){
				        /*tracy::s_wasActive = true;*/
				    }
				}
				arcan_shmif_unlock(&C);
				s_redraw();
				s_mainThreadTasks->Run();
		}
}

void Backend::Attention()
{
    arcan_event ev;
		memset(&ev, '\0', sizeof(struct arcan_event));
		ev.category = EVENT_EXTERNAL;
		ev.ext.kind = EVENT_EXTERNAL_ALERT;
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
/* if icon subsegment is there, forward */
}

void Backend::SetTitle(const char* title)
{
	arcan_event ev;
	memset(&ev, '\0', sizeof(struct arcan_event));
	ev.category = EVENT_EXTERNAL;
	ev.ext.kind = EVENT_EXTERNAL_IDENT;
	snprintf((char*)ev.ext.message.data, sizeof(ev.ext.message.data), "%s", title);
	arcan_shmif_enqueue(&C, &ev);
}

float Backend::GetDpiScale()
{
	return 1.0;
}

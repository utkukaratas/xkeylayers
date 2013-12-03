/******************************************************************************
 * xkeylayers
 *****************************************************************************/

#include <stdarg.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>

#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/extensions/record.h>
#include <X11/extensions/XTest.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>

#include <iostream>
#include <vector>
#include "config.hpp"

class TApplication
{
public: // system related
    // todo: wrap these away so that this module contains only the application
    // logic
    Display* _data_conn;
    Display* _ctrl_conn;
    XRecordContext _record_ctx;
    pthread_t _sigwait_thread;
    sigset_t _sigset;

    void trigger_key(TModifierKey *mod);
public: // app. logic related
    bool debug;
    TModifierKeys modifiers;
    vector<pair<KeySym, timeval>> last_presses; // a log of last N presses
                                                // for heuristics
    TKey* generated;
    timeval timeout;
    bool timeout_valid;

    void connect_x11();
    void hook_x11();
};

TApplication app;

/******************************************************************************
 * utilities
 *****************************************************************************/

void debug(const char *fmt, ...) {
    if (app.debug) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stdout, fmt, args);
        va_end(args);
    }
}

inline KeySym sym_from_code(KeyCode code) {
    return XkbKeycodeToKeysym(app._ctrl_conn, code, 0, 0);
}

/******************************************************************************
 * x11 setup
 *****************************************************************************/

void intercept(XPointer user_data, XRecordInterceptData* data);

void TApplication::trigger_key(TModifierKey* mod)
{
    for (auto keycode : mod->fake_keys)
    {
        ::debug("Generating %s!\n", XKeysymToString(sym_from_code(keycode)));

        XTestFakeKeyEvent(_ctrl_conn, keycode, True, 0);
        generated = key_add_key(generated, keycode);
    }
    for (auto keycode : mod->fake_keys)
    {
        XTestFakeKeyEvent(_ctrl_conn, keycode, False, 0);
        generated = key_add_key(generated, keycode);
    }
    XFlush(_ctrl_conn);
}

void TApplication::connect_x11() {
    int dummy;
    _data_conn = XOpenDisplay(NULL);
    _ctrl_conn = XOpenDisplay(NULL);

    if (!_data_conn || !_ctrl_conn)
    {
        cerr << "Unable to connect to X11 display. Is $DISPLAY set?\n";
        exit(EXIT_FAILURE);
    }
    if (!XQueryExtension(_ctrl_conn,
                "XTEST", &dummy, &dummy, &dummy))
    {
        cerr << "Xtst extension missing\n";
        exit(EXIT_FAILURE);
    }
    if (!XRecordQueryVersion(_ctrl_conn, &dummy, &dummy))
    {
        cerr << "Failed to obtain xrecord version\n";
        exit(EXIT_FAILURE);
    }
    if (!XkbQueryExtension(_ctrl_conn, &dummy, &dummy,
            &dummy, &dummy, &dummy))
    {
        cerr << "Failed to obtain xkb version\n";
        exit(EXIT_FAILURE);
    }
}

void TApplication::hook_x11() {
    XRecordRange *rec_range = XRecordAllocRange();
    rec_range->device_events.first = KeyPress;
    rec_range->device_events.last = ButtonRelease;
    XRecordClientSpec client_spec = XRecordAllClients;

    _record_ctx = XRecordCreateContext(_ctrl_conn,
            0, &client_spec, 1, &rec_range, 1);

    if (_record_ctx == 0)
    {
        cerr << "Failed to create xrecord context\n";
        exit(EXIT_FAILURE);
    }

    XSync(_ctrl_conn, False);

    if (!XRecordEnableContext(_data_conn,
                _record_ctx, intercept, (XPointer)&app))
    {
        cerr << "Failed to enable xrecord context\n";
        exit(EXIT_FAILURE);
    }

    if (!XRecordFreeContext(_ctrl_conn, _record_ctx))
    {
        cerr << "Failed to free xrecord context\n";
    }
}

/******************************************************************************
 * layering logic
 *****************************************************************************/

void handle_modifier(TModifierKey *mod, bool mouse_pressed, int key_event);

// this is the main hook to the X11. every input event is processed in this func.
void intercept(XPointer user_data, XRecordInterceptData* data)
{
    static bool mouse_pressed = False;

    if (data->category == XRecordFromServer)
    {
        xEvent* event = reinterpret_cast<xEvent*>(data->data);
        int     key_event = event->u.u.type;
        KeyCode key_code  = event->u.u.detail;
        debug("Intercepted key event %d, key code %d\n", key_event, key_code);

        TKey *g, *g_prev = NULL;

        for (g = app.generated; g != NULL; g = g->next)
        {
            if (g->key == key_code)
            {
                debug("Ignoring generated event.\n");
                if (g_prev != NULL)
                {
                    g_prev->next = g->next;
                }
                else
                {
                    app.generated = g->next;
                }
                free (g);
                goto exit;
            }
            g_prev = g;
        }

        // log last N keys for heuristics
        // todo: make this a circular buffer or something
        if (key_event == KeyRelease) {
            timeval now;
            gettimeofday(&now, NULL);
            auto p = make_pair(sym_from_code(key_code), now);
            app.last_presses.push_back(p);
        }

        if (key_event == ButtonPress)
        {
            mouse_pressed = True;
        }
        else if (key_event == ButtonRelease)
        {
            mouse_pressed = False;
        }
        else
        {
            for (TModifierKey* mod : app.modifiers) {
                if ((mod->use_keycode == False && sym_from_code(key_code) == mod->keysym)
                    ||
                    (mod->use_keycode == True && key_code == mod->keycode))
                {
                    handle_modifier(mod, mouse_pressed, key_event);
                }
                else if (mod->is_down && key_event == KeyPress)
                {
                    mod->used = True;
                }
            }
        }
    }

exit:
    XRecordFreeData(data);
}

bool is_any_mod_down() {
    return std::any_of(app.modifiers.begin(),
                       app.modifiers.end(),
                       [](TModifierKey* m){
        return m->is_down;
    });
}

// testing heuristics ideas...
// this one looks if the last 3 immediate keypresses are alpha or not.
// if the last 3 immediate key events are alphanumeric, most likely user wants
// the space key.
bool heuristic_test1(TModifierKey* mod) {
    auto& lst = app.last_presses;
    bool last_3_keys_alpha =
            std::all_of(lst.rbegin() + 1, // the very last event was for the
                                        // modifier itself, so skip that.
                        lst.rbegin() + 4,
                        [](pair<KeySym, timeval> p){
                            KeySym k = p.first;
                            return ((k >= XK_A && k <= XK_Z) ||
                                    (k >= XK_a && k <= XK_z));
                        }
            );

    // take a look if the space was pressed very recently... (1 sec)
    timeval now;
    gettimeofday(&now, NULL);
    bool is_immediate = abs(diff_ms(now, lst[lst.size()-1].second)) < 1000;
    return last_3_keys_alpha && is_immediate;
}

// handle_modifier is called everytime a modifier key press/release occurs.
void handle_modifier(TModifierKey* mod, bool mouse_pressed, int key_event)
{
    if (key_event == KeyPress)
    {
        debug("Key pressed!\n");
        mod->is_down = True;

        if (app.timeout_valid)
            gettimeofday(&mod->down_at, NULL);

        if (mouse_pressed)
        {
            mod->used = True;
        }

        // testing heuristic 1
        if (heuristic_test1(mod)) {
            // if heuristic func returns true this means user probably wants
            // the key itself (space) not the modifier.
            app.trigger_key(mod);
            mod->used = true;
        }
    }
    else
    {
        debug("Key released!\n");
        if (!mod->used)
        {
            timeval down_time = app.timeout;
            if (app.timeout_valid)
            {
                gettimeofday(&down_time, NULL);
                timersub(&down_time, &mod->down_at, &down_time);
            }

            if (!app.timeout_valid ||
                timercmp(&down_time, &app.timeout, <))
            {
                // so a modifier key was pressed shortly.
                // generate fake keys (actually... press and release events).
                app.trigger_key(mod);
            }
        }
        mod->used = False;
        mod->is_down = False;
    }
}

/******************************************************************************
 * main
 *****************************************************************************/

void* sig_handler(void *user_data)
{
    TApplication *self = (TApplication*)user_data;
    int sig;

    debug("sig_handler running...\n");

    sigwait(&self->_sigset, &sig);

    debug("Caught signal %d!\n", sig);

    if (!XRecordDisableContext(self->_ctrl_conn, self->_record_ctx))
    {
        fprintf(stderr, "Failed to disable xrecord context\n");
        exit(EXIT_FAILURE);
    }

    XSync(self->_ctrl_conn, False);

    debug("sig_handler exiting...\n");

    return NULL;
}

int main(int argc, char **argv)
{
    int ch;
    app.debug = False;
    app.timeout.tv_sec = 0;
    app.timeout.tv_usec = 500000;
    app.timeout_valid = True;
    app.generated = NULL;

    while ((ch = getopt (argc, argv, "d")) != -1)
    {
        switch (ch)
        {
        case 'd':
            app.debug = True;
            break;
        default:
            printf("Usage: %s [-d]\n", argv[0]);
            printf("Runs as a daemon unless -d flag is set\n");
            return EXIT_SUCCESS;
        }
    }

    app.connect_x11();

    try {
       app.modifiers = load_config(".xkeylayers");
    }
    catch (...) {
        debug("configuration failure. exiting!");
        exit(EXIT_FAILURE);
    }

    if (app.modifiers.size() == 0) {
        debug("no modifiers declared. exiting!");
        exit(EXIT_FAILURE);
    }

    if (!app.debug)
        daemon (0, 0);

    // setup signal handler for proper exits.
    sigemptyset(&app._sigset);
    sigaddset(&app._sigset, SIGINT);
    sigaddset(&app._sigset, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &app._sigset, NULL);
    pthread_create(&app._sigwait_thread, NULL, sig_handler, &app);

    // hook 11 and start processing input events. this call will block.
    app.hook_x11();

    XCloseDisplay(app._ctrl_conn);
    XCloseDisplay(app._data_conn);

    debug("main exiting\n");

    return EXIT_SUCCESS;
}

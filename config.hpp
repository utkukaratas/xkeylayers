/******************************************************************************
 * xxx
 *****************************************************************************/
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <fstream>
#include "picojson.h"

using namespace std;

/******************************************************************************
 * internal types
 *****************************************************************************/

struct TKey
{
    KeyCode key;
    TKey* next;
};

struct TModifierKey {
    bool use_keycode;          // we can either use keycode or keysym
    KeyCode keycode;           // incoming keycode
    KeySym keysym;             // incoming keysym
    vector<KeyCode> fake_keys; // fake keys to generate instead of (keycode/keysym)
    bool used;
    bool is_down;
    bool mouse;
    timeval down_at;
};

typedef vector<TModifierKey*> TModifierKeys;

/******************************************************************************
 * configuration loader
 *****************************************************************************/

TModifierKeys load_config(string filename) {
    //    ifstream f(filename);
    //    if (!f.is_open()) {
    //        cout << "cant open config file";
    //    }
    //    picojson::value root;
    //    f >> root;
    //    std::string err = picojson::get_last_error();
    //    if (! err.empty()) {
    //        std::cerr << err << std::endl;
    //    }

    //    // check if the type of the value is "object"
    //    if (! root.is<picojson::object>()) {
    //        std::cerr << "JSON is not an object" << std::endl;
    //        exit(EXIT_FAILURE);
    //    }
    TModifierKeys result;

        //xxx
        TModifierKey *km = new TModifierKey;
        km->keycode = 65; km->use_keycode = true;
        km->fake_keys.push_back(8);//sym_from_code(XStringToKeysym("space")));
        result.push_back(km);

        return result;
    //    const picojson::value& layer0 = root.get("layer0");
    //    for(auto keydef : layer0.get<picojson::array>()) {
    //        auto pair = keydef.get<picojson::object>().begin();
    //        string thekey = pair->first;
    //        picojson::value& theaction = pair->second;
    //        cout << thekey << theaction.get("push").get<string>();

    //        TSpecialKey *km = new TSpecialKey;
    //        if (thekey.size() > 0 && thekey[0] == '#') {
    //            if (!special_from_keycodestr(km, thekey))
    //                exit(EXIT_FAILURE);
    //        }
    //        else {
    //            if (!special_from_keysymstr(km, thekey))
    //                exit(EXIT_FAILURE);
    //        }

    //        int ms = atoi (optarg);
    //        if (ms > 0)
    //        {
    //            app->timeout_valid = True;
    //            app->timeout.tv_sec = ms / 1000;
    //            app->timeout.tv_usec = (ms % 1000) * 1000;
    //        }
    //        else
    //        {
    //            app->timeout_valid = False;
    //        }


    //        for(;;)
    //        {
    //            key = strsep (&to, "|");
    //            if (key == NULL)
    //                break;

    //            if ((ks = XStringToKeysym (key)) == NoSymbol)
    //            {
    //                fprintf (stderr, "Invalid key: %s\n", key);
    //                return NULL;
    //            }

    //            code = XKeysymToKeycode (dpy, ks);
    //            if (code == 0)
    //            {
    //                fprintf (stderr, "WARNING: No keycode found for keysym "
    //                         "%s (0x%x) in mapping %s. Ignoring this "
    //                         "mapping.\n", key, (unsigned int)ks, token);
    //                return NULL;
    //            }
    //            km->to_keys = key_add_key (km->to_keys, code);

    //            if (debug)
    //            {
    //                fprintf(stderr, "to \"%s\" (keysym 0x%x, key code %d)\n",
    //                        key,
    //                        (unsigned) XStringToKeysym (key),
    //                        (unsigned) code);
    //            }
    //        }
    //    }
}


//bool special_from_keycodestr(TModifierKey *mod, string str) {
//    errno = 0;
//    long fromcode = strtoul (str.c_str(), NULL, 0); // dec, oct, hex automatically
//    if (errno == 0
//        && fromcode <=255
//        && sym_from_code((KeyCode)fromcode) != NoSymbol)
//    {
//        mod->use_keycode = true;
//        mod->keycode = (KeyCode) fromcode;
//        if (app.debug)
//        {
//          KeySym ks_temp = sym_from_code((KeyCode)fromcode);
//          fprintf(stderr, "Assigned mapping from from \"%s\" (keysym 0x%x, "
//                  "key code %d)\n",
//                  XKeysymToString(ks_temp),
//                  (unsigned) ks_temp,
//                  (unsigned) mod->keycode);
//        }
//        return true;
//    }
//    else
//    {
//        cerr << "Invalid keycode: " << str << endl;
//        return false;
//    }

//}

//bool special_from_keysymstr(TModifierKey *mod, string str) {
//    KeySym    ks;
//    if ((ks = XStringToKeysym (str.c_str())) == NoSymbol)
//    {
//        cerr << "Invalid key: " << str << endl;
//        return false;
//    }

//    mod->use_keycode  = false;
//    mod->keysym     = ks;

//    if (app.debug)
//    {
//        fprintf(stderr, "Assigned mapping from \"%s\" (keysym 0x%x, "
//                "key code %d)\n",
//                XKeysymToString(mod->keysym),
//                (unsigned) mod->keysym,
//                (unsigned) XKeysymToKeycode(app._ctrl_conn, mod->keysym));
//    }
//    return true;
//}

/******************************************************************************
 * misc. utilities
 *****************************************************************************/

TKey *key_add_key(TKey *keys, KeyCode key)
{
    TKey *rval = keys;

    if (keys == NULL)
    {
        keys = new TKey;
        rval = keys;
    }
    else
    {
        while (keys->next != NULL) keys = keys->next;
        keys = (keys->next = new TKey);
    }

    keys->key = key;
    keys->next = NULL;

    return rval;
}

static int diff_ms(timeval t1, timeval t2) {
    return ( ((t1.tv_sec - t2.tv_sec) * 1000000)
             + (t1.tv_usec - t2.tv_usec) ) / 1000;
}

#endif // CONFIG_HPP

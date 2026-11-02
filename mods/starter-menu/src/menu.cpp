// ClientDaBell: a hand menu built from the starter menu (ii's Stupid Menu layout).
// The layout idea comes from ii's menu; the code is new.
//   Hold Y (left hand)      -> menu on your left hand, tipped like ii's; turn your palm to you
//                              (B and the right hand with "Right Hand Menu")
//   Touch a button with your other index finger to press it
//   < and > on the sides    -> previous / next page
//   Pages: Movement Mods, Rig Mods, Items, Settings, Enabled Mods
// Movement only changes your own game. Rig mods (dances) and Items are seen by the room,
// within the limits in AGENTS.md ("Things others see"). Read docs/GUIDE.md before adding.
#include "ymsdk.hpp"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <dirent.h>
#include <strings.h>
#include <ctime>
#include <string>
#include <vector>

using namespace BNM;
using namespace BNM::Structures::Unity;
using BNM::IL2CPP::Il2CppObject;

YEEPS_MOD("Nova X")
#define NX_VERSION "v0.0.1"

// ---- small helpers ---------------------------------------------------------

static std::string TypeName(const BNM::IL2CPP::Il2CppType *t) {
    Class c(t);
    if (!c.IsValid() || !c.GetClass() || !c.GetClass()->name) return "?";
    auto *k = c.GetClass();
    return (k->namespaze && k->namespaze[0]) ? std::string(k->namespaze) + "." + k->name : k->name;
}
// Pick an overload by its exact parameter types, e.g. GetComponent(System.Type).
static MethodBase Overload(Class c, const char *name, std::vector<std::string> types) {
    for (auto &m : c.GetMethods(false)) {
        auto *mi = m.GetInfo();
        if (!mi || !mi->name || strcmp(mi->name, name) != 0 || mi->parameters_count != types.size()) continue;
        bool ok = true;
        for (size_t i = 0; i < types.size() && ok; i++) ok = TypeName(mi->parameters[i]) == types[i];
        if (ok) return m;
    }
    return {};
}
// Find a method by name and argument count on a class or any class above it
// (Photon's Room keeps CustomProperties on its parent, RoomInfo). argc -1 = any.
static MethodBase FindUp(Class c, const char *name, int argc) {
    for (int depth = 0; c.IsValid() && c.GetClass() && depth < 8; depth++) {
        for (auto &m : c.GetMethods(false)) {
            auto *mi = m.GetInfo();
            if (mi && mi->name && strcmp(mi->name, name) == 0 && (argc < 0 || mi->parameters_count == argc)) return m;
        }
        auto *parent = c.GetClass()->parent;
        if (!parent) break;
        c = Class(&parent->byval_arg);
    }
    return {};
}

// Unity objects we keep between frames must be held by a GC handle, or the
// garbage collector can free them under us.
static void *(*p_handleNew)(void *, bool);
static void *(*p_handleTarget)(void *);
static void (*p_handleFree)(void *);

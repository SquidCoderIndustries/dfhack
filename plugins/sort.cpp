#include "Debug.h"
#include "LuaTools.h"
#include "PluginManager.h"
#include "PluginLua.h"

#include "modules/Gui.h"
#include "modules/Units.h"

#include "df/gamest.h"
#include "df/unit.h"
#include "df/widget_unit_list.h"
#include "df/world.h"

using std::vector;
using std::string;

using namespace DFHack;

DFHACK_PLUGIN("sort");

REQUIRE_GLOBAL(game);
REQUIRE_GLOBAL(world);

namespace DFHack {
    DBG_DECLARE(sort, log, DebugCategory::LINFO);
}

using item_or_unit = std::pair<void *, bool>;
using filter_vec_type = std::vector<std::function<bool(item_or_unit)>>;

// recreated here since our autogenerated df::sort_entry lacks template params
struct sort_entry {
    std::function<bool(const item_or_unit &, const item_or_unit &)> fn;
    string ident;
};

static const string DFHACK_SORT_IDENT = "dfhack_sort";

//
// filter logic
//

static bool probing = false;
static bool probe_result = false;

static bool do_filter(const char *module_name, const char *fn_name, const item_or_unit &elem) {
    if (elem.second) return true;
    auto unit = (df::unit *)elem.first;

    if (probing) {
        TRACE(log).print("probe successful\n");
        probe_result = true;
        return false;
    }

    bool ret = true;
    color_ostream &out = Core::getInstance().getConsole();
    Lua::CallLuaModuleFunction(out, module_name, fn_name, std::make_tuple(unit),
        1, [&](lua_State *L){
            ret = lua_toboolean(L, 1);
        }
    );
    TRACE(log).print("filter result for %s: %d\n", Units::getReadableName(unit).c_str(), ret);
    return !ret;
}

static bool do_squad_filter(item_or_unit elem) {
    return do_filter("plugins.sort", "do_squad_filter", elem);
}

static bool do_justice_filter(item_or_unit elem) {
    return do_filter("plugins.sort.info", "do_justice_filter", elem);
}

static bool do_work_animal_assignment_filter(item_or_unit elem) {
    return do_filter("plugins.sort.info", "do_work_animal_assignment_filter", elem);
}

static int32_t our_filter_idx(df::widget_unit_list *unitlist) {
    if (world->units.active.empty())
        return true;

    df::unit *u = world->units.active[0]; // any unit will do; we just need a sentinel
    if (!u)
        return true;

    probing = true;
    probe_result = false;
    int32_t idx = 0;

    filter_vec_type *filter_vec = reinterpret_cast<filter_vec_type *>(&unitlist->filter_func);

    TRACE(log).print("probing for our filter function\n");
    for (auto fn : *filter_vec) {
        fn(std::make_pair(u, false));
        if (probe_result) {
            TRACE(log).print("found our filter function at idx %d\n", idx);
            break;
        }
        ++idx;
    }

    probing = false;
    return probe_result ? idx : -1;
}

static df::widget_unit_list * get_squad_unit_list() {
    return virtual_cast<df::widget_unit_list>(
        Gui::getWidget(&game->main_interface.unit_selector, "Unit selector"));
}

static df::widget_container * get_justice_panel(const char *which) {
    auto tabs = virtual_cast<df::widget_container>(
        Gui::getWidget(&game->main_interface.info.justice, "Tabs"));
    if (!tabs) return NULL;
    auto open_cases = virtual_cast<df::widget_container>(Gui::getWidget(tabs, which));
    if (!open_cases) return NULL;
    return virtual_cast<df::widget_container>(Gui::getWidget(open_cases, "Right panel"));
}

static df::widget_unit_list * get_interrogate_unit_list(const char *which) {
    auto right_panel = get_justice_panel(which);
    if (!right_panel) return NULL;
    return virtual_cast<df::widget_unit_list>(Gui::getWidget(right_panel, "Interrogate"));
}

static df::widget_unit_list * get_convict_unit_list(const char *which) {
    auto right_panel = get_justice_panel(which);
    if (!right_panel) return NULL;
    return virtual_cast<df::widget_unit_list>(Gui::getWidget(right_panel, "Convict"));
}

static df::widget_unit_list * get_work_animal_assignment_unit_list() {
    auto tabs = virtual_cast<df::widget_container>(
        Gui::getWidget(&game->main_interface.info.creatures, "Tabs"));
    if (!tabs) return NULL;
    auto pets = virtual_cast<df::widget_container>(Gui::getWidget(tabs, "Pets/Livestock"));
    if (!pets) return NULL;
    return virtual_cast<df::widget_unit_list>(Gui::getWidget(pets, "Hunting assignment"));
}

//
// sorting logic
//

static bool sort_proxy(const item_or_unit &a, const item_or_unit &b) {
    if (a.second || b.second)
        return true;

    bool ret = true;
    color_ostream &out = Core::getInstance().getConsole();
    Lua::CallLuaModuleFunction(out, "plugins.sort", "do_sort",
        std::make_tuple((df::unit *)a.first, (df::unit *)b.first),
        1, [&](lua_State *L){
            ret = lua_toboolean(L, 1);
        }
    );
    return ret;
}

static sort_entry do_sort{
    sort_proxy,
    DFHACK_SORT_IDENT
};

int32_t our_sort_idx(const std::vector<sort_entry> &sorting_by) {
    for (size_t i = 0; i < sorting_by.size(); ++i) {
        if (sorting_by[i].ident == DFHACK_SORT_IDENT) {
            return (int32_t)i;
        }
    }
    return -1;
}

//
// plugin logic
//

DFhackCExport command_result plugin_init(color_ostream &out, vector<PluginCommand> &commands) {
    return CR_OK;
}

static void remove_filter_function(color_ostream &out, const char *which, df::widget_unit_list *unitlist) {
    int32_t idx = our_filter_idx(unitlist);
    if (idx >= 0) {
        DEBUG(log,out).print("removing %s filter function\n", which);
        filter_vec_type *filter_vec = reinterpret_cast<filter_vec_type *>(&unitlist->filter_func);
        vector_erase_at(*filter_vec, idx);
    }
}

static void remove_sort_function(color_ostream &out, const char *which, df::widget_unit_list *unitlist) {
    std::vector<sort_entry> *sorting_by = reinterpret_cast<std::vector<sort_entry> *>(&unitlist->sorting_by);
    int32_t idx = our_sort_idx(*sorting_by);
    if (idx >= 0) {
        DEBUG(log).print("removing %s sort function\n", which);
        vector_erase_at(*sorting_by, idx);
    }
}

DFhackCExport command_result plugin_shutdown(color_ostream &out) {
    if (auto unitlist = get_squad_unit_list()) {
        remove_filter_function(out, "squad", unitlist);
        remove_sort_function(out, "squad", unitlist);
    }

    if (auto unitlist = get_interrogate_unit_list("Open cases"))
        remove_filter_function(out, "open cases interrogate", unitlist);
    if (auto unitlist = get_interrogate_unit_list("Cold cases"))
        remove_filter_function(out, "cold cases interrogate", unitlist);

    if (auto unitlist = get_convict_unit_list("Open cases"))
        remove_filter_function(out, "open cases convict", unitlist);
    if (auto unitlist = get_convict_unit_list("Cold cases"))
        remove_filter_function(out, "cold cases convict", unitlist);

    if (auto unitlist = get_work_animal_assignment_unit_list())
        remove_filter_function(out, "work animal assignment", unitlist);

    return CR_OK;
}

//
// Lua API
//

static void sort_set_squad_filter_fn(color_ostream &out) {
    auto unitlist = get_squad_unit_list();
    if (unitlist && our_filter_idx(unitlist) == -1) {
        DEBUG(log).print("adding squad filter function\n");
        auto filter_vec = reinterpret_cast<filter_vec_type *>(&unitlist->filter_func);
        filter_vec->emplace_back(do_squad_filter);
        DEBUG(log).print("clearing partitions\n"); // removes sorting other squads to end
        auto partitions_vec = reinterpret_cast<filter_vec_type *>(&unitlist->partitions);
        partitions_vec->clear();
        unitlist->sort_flags.bits.NEEDS_RESORTED = true;
    }
}

static void sort_set_justice_filter_fn(color_ostream &out, df::widget_unit_list *unitlist) {
    if (unitlist && our_filter_idx(unitlist) == -1) {
        DEBUG(log).print("adding justice filter function\n");
        auto filter_vec = reinterpret_cast<filter_vec_type *>(&unitlist->filter_func);
        filter_vec->emplace_back(do_justice_filter);
        unitlist->sort_flags.bits.NEEDS_RESORTED = true;
    }
}

static void sort_set_work_animal_assignment_filter_fn(color_ostream &out, df::widget_unit_list *unitlist) {
    if (unitlist && our_filter_idx(unitlist) == -1) {
        DEBUG(log).print("adding work animal assignment filter function\n");
        auto filter_vec = reinterpret_cast<filter_vec_type *>(&unitlist->filter_func);
        filter_vec->emplace_back(do_work_animal_assignment_filter);
        unitlist->sort_flags.bits.NEEDS_RESORTED = true;
    }
}

static void sort_set_sort_fn(color_ostream &out) {
    auto unitlist = get_squad_unit_list();
    if (!unitlist)
        return;
    DEBUG(log).print("adding squad sort function\n");
    std::vector<sort_entry> *sorting_by = reinterpret_cast<std::vector<sort_entry> *>(&unitlist->sorting_by);
    sorting_by->clear();
    sorting_by->emplace_back(do_sort);
    unitlist->sort_flags.bits.NEEDS_RESORTED = true;
}

static bool sort_get_sort_active(color_ostream &out) {
    auto unitlist = get_squad_unit_list();
    if (!unitlist)
        return false;
    std::vector<sort_entry> *sorting_by = reinterpret_cast<std::vector<sort_entry> *>(&unitlist->sorting_by);
    return our_sort_idx(*sorting_by) >= 0;
}

static bool sort_is_interviewed(color_ostream &out, df::unit *unit) {
    auto flag_map = reinterpret_cast<std::unordered_map<df::unit *,df::justice_screen_interrogation_list_flag> *>(&game->main_interface.info.justice.crimeflag);
    if (!flag_map->contains(unit))
        return false;
    return (*flag_map)[unit].bits.ALREADY_INTERVIEWED;
}

DFHACK_PLUGIN_LUA_FUNCTIONS{
    DFHACK_LUA_FUNCTION(sort_set_squad_filter_fn),
    DFHACK_LUA_FUNCTION(sort_set_justice_filter_fn),
    DFHACK_LUA_FUNCTION(sort_set_work_animal_assignment_filter_fn),
    DFHACK_LUA_FUNCTION(sort_set_sort_fn),
    DFHACK_LUA_FUNCTION(sort_get_sort_active),
    DFHACK_LUA_FUNCTION(sort_is_interviewed),
    DFHACK_LUA_END
};

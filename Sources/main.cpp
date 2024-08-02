#include <3ds.h>
#include "csvc.h"
#include <CTRPluginFramework.hpp>

#include <vector>

namespace CTRPluginFramework {


// This function is called before main and before the game starts
// Useful to do code edits safely
void PatchProcess(FwkSettings &settings) {


}

// This function is called when the process exits
// Useful to save settings, undo patchs or clean up things
void OnProcessExit(void) {

  

}

void InitMenu(PluginMenu &menu)
{


}

int main(void) {
  PluginMenu menu{ "ctrpf plugin", 0, 7, 4 };

  menu.SynchronizeWithFrame(true);

  InitMenu(menu);

  return menu.Run();
}


} // namespace CTRPluginFramework

#include "components/gui_button.h"
#include "road_config.h"
#include "../simtool.h"
#include "../boden/wege/strasse.h"

#define L_DIALOG_WIDTH (200)

uint8 road_config_frame_t::street_flag = 0;

road_config_frame_t::road_config_frame_t(player_t* player_, tool_build_way_t* tool_) :
  gui_frame_t(translator::translate("configure_road"))
  {
    player = player_;
    tool = tool_;
    street_flag = tool->get_street_flag();
    
    scr_coord cursor(D_MARGIN_LEFT, D_MARGIN_TOP);
    
    button.init(button_t::square_state, "avoid becoming cityroad", cursor);
    button.set_width(L_DIALOG_WIDTH - D_MARGINS_X);
    button.add_listener(this);
    button.pressed = street_flag&strasse_t::AVOID_CITYROAD;
    add_component(&button);
    cursor.y += button.get_size().h + D_V_SPACE;
    
    set_windowsize(scr_size(L_DIALOG_WIDTH, D_TITLEBAR_HEIGHT + cursor.y + D_MARGIN_BOTTOM));
  }
  
  bool road_config_frame_t::action_triggered(gui_action_creator_t *komp, value_t) {
    if(komp==&button) {
      button.pressed = !button.pressed;
      if(button.pressed) {
        street_flag |= strasse_t::AVOID_CITYROAD;
      } else {
        street_flag &= ~(strasse_t::AVOID_CITYROAD);
      }
      tool->set_street_flag(street_flag);
    }
    return true;
  }
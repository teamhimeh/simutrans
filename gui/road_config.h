#ifndef road_config_h
#define road_config_h

#include "gui_frame.h"
#include "components/action_listener.h"

class button_t;
class tool_build_way_t;
class player_t;

class road_config_frame_t : public gui_frame_t, private action_listener_t {
private:
  static uint8 street_flag;
  player_t* player;
  tool_build_way_t* tool;
  button_t button;
  
public:
  road_config_frame_t(player_t *, tool_build_way_t*);
  bool action_triggered(gui_action_creator_t*, value_t) OVERRIDE;
  const char* get_help_filename() const { return "road_config.txt"; }
};

#endif
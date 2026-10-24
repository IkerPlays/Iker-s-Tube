#include "ui/ui.hpp"
#include <cstdio>
// Regression test : a tap on every button of a SelectorView must select that button (a release, touch_x = -1, once counted as a
// tap on button 0). Build and run: ./selector_test.sh
int main(){
  int bad=0;
  var_round_buttons=true;
  SelectorView *s=(new SelectorView(0,0,320,35,false));
  s->set_texts({std::string("Audio"),std::string("144p"),std::string("240p"),std::string("360p"),std::string("480p")},3);
  s->set_title("Video");
  int calls=0; s->set_on_change([&](const SelectorView&v){calls++;});
  for(int target=0;target<5;target++){
    double x=s->button_x_left(target)+s->button_x_size()/2;
    Hid_info k; k.touch_x=x; k.touch_y=20; k.p_touch=true; s->update(k);
    k.p_touch=false; s->update(k); 
    Hid_info r; r.touch_x=-1; r.touch_y=-1; r.p_touch=false; s->update(r);
    printf("target %d -> selected %d calls %d\n",target,s->selected_button,calls); if(s->selected_button!=target) bad++;
  }
  puts(bad?"FAIL":"OK");
  return bad;
}

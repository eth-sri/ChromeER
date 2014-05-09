#include "event_racer_log_host.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include <algorithm>

namespace content {

EventAction::EventAction(unsigned int id) : id_(id) {}

EventAction::~EventAction() {}

void EventAction::AddEdge(unsigned int dst) {
  if (std::find(edges_.begin(), edges_.end(), dst) == edges_.end())
    edges_.push_back(dst);
}

uint32 EventRacerLogHost::next_event_racer_log_id_ = 1;

EventRacerLogHost::EventRacerLogHost()
 : id_ (next_event_racer_log_id_++) {
}

EventRacerLogHost::~EventRacerLogHost() {}

EventAction *EventRacerLogHost::CreateEventAction(unsigned int id) {
  scoped_ptr<EventAction> a(new EventAction(id));
  EventAction *ptr = a.get();
  actions_.set(id, a.Pass());
  return ptr;
}

void EventRacerLogHost::CreateEdge(unsigned int srcid, unsigned int dstid) {
  EventAction *src = actions_.get(srcid);
  src->AddEdge(dstid);
}

void EventRacerLogHost::WriteDot(scoped_ptr<EventRacerLogHost> log, int32 site_id) {
  std::string dotsrc = "digraph ER {\n";
  for (ActionsMapType::const_iterator i = log->actions_.begin(); i != log->actions_.end(); ++i) {
    EventAction *a = i->second;
    for (std::vector<unsigned int>::const_iterator j = a->begin(); j != a->end(); ++j) {
      base::StringAppendF(&dotsrc, "n%u -> n%u\n", a->GetId(), *j);
    }
  }
  dotsrc += "}\n";
  base::FilePath path(
    base::StringPrintf("eventracer-id%02u-site%02d.log", log->GetId(), site_id));
  base::File file(path, base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
  file.WriteAtCurrentPos(dotsrc.data(), dotsrc.size());
}

} // end namespace

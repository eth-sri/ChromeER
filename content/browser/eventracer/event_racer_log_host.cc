#include "event_racer_log_host.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "content/common/event_racer_messages.h"
#include <algorithm>

namespace content {

namespace detail {

const char * Operation::typestr[] = { "", "", "read: ", "write: ",
                                      "trigger: ", "value: ", "other: " };

const char *Operation::GetTypeStr() const {
  if (type_ >= OTHER)
    return typestr[OTHER];
  else
    return typestr[type_];
}

EventAction::EventAction(unsigned int id) : id_(id) {}

EventAction::~EventAction() {}

void EventAction::AddEdge(unsigned int dst) {
  if (std::find(edges_.begin(), edges_.end(), dst) == edges_.end())
    edges_.push_back(dst);
}

void EventAction::AddOperation(unsigned int type, size_t loc) {
  ops_.push_back(Operation(Operation::Type(type), loc));
}

} // end namespace detail

uint32 EventRacerLogHost::next_event_racer_log_id_ = 1;

EventRacerLogHost::EventRacerLogHost(int32 routing_id)
  : id_ (next_event_racer_log_id_++),
    routing_id_(routing_id),
    strings_(1) {
}

EventRacerLogHost::~EventRacerLogHost() {
}

EventRacerLogHost::EventAction *EventRacerLogHost::CreateEventAction(unsigned int id) {
  scoped_ptr<EventAction> a(new EventAction(id));
  EventAction *ptr = a.get();
  actions_.set(id, a.Pass());
  return ptr;
}

void EventRacerLogHost::CreateEdge(unsigned int srcid, unsigned int dstid) {
  EventAction *src = actions_.get(srcid);
  src->AddEdge(dstid);
}

void EventRacerLogHost::UpdateStringTable(size_t index, const std::vector<std::string> &v) {
  LOG(INFO) << "UpdateStringTable: index = " << index
            << " size = " << strings_.size() << "\n";

  assert(index == strings_.size());
  strings_.insert(strings_.end(), v.begin(), v.end());
}

bool EventRacerLogHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EventRacerLogHost, msg)
    IPC_MESSAGE_HANDLER(EventRacerLogHostMsg_CompletedEventAction, OnCompletedEventAction)
    IPC_MESSAGE_HANDLER(EventRacerLogHostMsg_HappensBefore, OnHappensBefore)
    IPC_MESSAGE_HANDLER(EventRacerLogHostMsg_UpdateStringTable, OnUpdateStringTable)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void EventRacerLogHost::OnCompletedEventAction(const blink::WebEventAction &wa) {
  EventAction *a = CreateEventAction(wa.id);
  for (size_t i = 0; i < wa.ops.size(); ++i)
    a->AddOperation(wa.ops[i].type, wa.ops[i].location);
}

void EventRacerLogHost::OnHappensBefore(const std::vector<blink::WebEventActionEdge> &v) {
  for (size_t i = 0; i < v.size(); ++i)
    CreateEdge(v[i].first, v[i].second);
}

void EventRacerLogHost::OnUpdateStringTable(size_t index, const std::vector<std::string> &v) {
  UpdateStringTable(index, v);
}


void EventRacerLogHost::WriteDot(scoped_ptr<EventRacerLogHost> log, int32 site_id) {
  std::string dotsrc = "digraph ER {\n";
  for (ActionsMapType::const_iterator i = log->actions_.begin(); i != log->actions_.end(); ++i) {
    EventAction *a = i->second;
    
    // Output event action node.
    base::StringAppendF(&dotsrc, "n%u[label=\"id: %u", a->GetId(), a->GetId());
    if (a->Ops().size()) {
      unsigned int level = 0;
      dotsrc += "\\l";
      EventAction::OpsType::const_iterator i;
      for (i = a->Ops().begin(); i != a->Ops().end(); ++i) {
        const Operation &op = *i;
        if (op.GetType() == Operation::EXIT_SCOPE) {
          --level;
        } else {
          base::StringAppendF(&dotsrc, "%*c%s", level*4, ' ', op.GetTypeStr());
          if (op.GetLocation()) {
            if (op.GetType() == Operation::TRIGGER_ARC)
              base::StringAppendF(&dotsrc, "%lu", op.GetLocation());
            else
              dotsrc += log->strings_[op.GetLocation()];
          }
          if (op.GetType() == Operation::ENTER_SCOPE)
            ++level;
          dotsrc += "\\l";
        }
      }
    }
    dotsrc += "\"\n]\n\n";

  // Output edges.
    EventAction::EdgesType::const_iterator j;
    for (j = a->Edges().begin(); j != a->Edges().end(); ++j) {
      base::StringAppendF(&dotsrc, "n%u -> n%u\n", a->GetId(), *j);
    }
  }
  dotsrc += "}\n";
  base::FilePath path(
    base::StringPrintf("eventracer-id%02u-site%02d.dot", log->GetId(), site_id));
  base::File file(path, base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
  file.WriteAtCurrentPos(dotsrc.data(), dotsrc.size());
}

} // end namespace

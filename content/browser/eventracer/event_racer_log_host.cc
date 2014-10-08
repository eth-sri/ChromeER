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
  // FIXME(chill):  if (std::find(edges_.begin(), edges_.end(), dst) == edges_.end())
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
    nedges_(0) {
  for (size_t k = 0; k < STRING_TABLE_KIND_COUNT; ++k) {
    nstrings_[k] = 1;
    strings_[k].push_back('\0');
  }
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
  ++nedges_;
}

void EventRacerLogHost::UpdateStringTable(size_t kind, const std::vector<std::string> &v) {
  assert(kind < STRING_TABLE_KIND_COUNT);
  size_t n = 0;
  std::vector<std::string>::const_iterator s;
  for (s = v.begin(); s != v.end(); ++s)
    n += s->size() + 1;
  std::vector<char> &strings = strings_[kind];
  strings.reserve(strings.size() + n);
  for (s = v.begin(); s != v.end(); ++s) {
    strings.insert(strings.end(), s->begin(), s->end());
    strings.push_back('\0');
  }
  nstrings_[kind] += v.size();
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

void EventRacerLogHost::OnUpdateStringTable(size_t kind, const std::vector<std::string> &v) {
  UpdateStringTable(kind, v);
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
        } else if (op.GetType() == Operation::ENTER_SCOPE) {
          base::StringAppendF(&dotsrc, "%*c%s", level*4, ' ', op.GetTypeStr());
          if (op.GetLocation()) {
            dotsrc += &log->strings_[SCOPE_STRINGS][op.GetLocation()];
          }
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

  // Output binary format too.
  WriteBin(log.Pass(), site_id);
}

namespace {
  template<typename T>
  void Write(base::File &file, T n) {
    file.WriteAtCurrentPos(reinterpret_cast<const char *>(&n), sizeof(n));
  }
}

void EventRacerLogHost::WriteBin(scoped_ptr<EventRacerLogHost> log, int32 site_id) {
  base::FilePath path(
    base::StringPrintf("eventracer-id%02u-site%02d.bin", log->GetId(), site_id));
  base::File file(path, base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);

  // Write the memory location strings.
  Write<uint32>(file, log->strings_[VAR_STRINGS].size());
  file.WriteAtCurrentPos(&log->strings_[VAR_STRINGS][0], log->strings_[VAR_STRINGS].size());
  Write<uint32>(file, log->nstrings_[VAR_STRINGS] * 2); // number of hash buckets

  // Write the scope strings.
  Write<uint32>(file, log->strings_[SCOPE_STRINGS].size());
  file.WriteAtCurrentPos(&log->strings_[SCOPE_STRINGS][0], log->strings_[SCOPE_STRINGS].size());
  Write<uint32>(file, log->nstrings_[SCOPE_STRINGS] * 2);

  // Write number of actions.
  Write<uint32>(file, log->actions_.size());

  // Write number of edges.
  Write<uint32>(file, log->nedges_);

  // Write edges.
  for (ActionsMapType::const_iterator i = log->actions_.begin(); i != log->actions_.end(); ++i) {
    EventAction *a = i->second;
    EventAction::EdgesType::const_iterator j;
    for (j = a->Edges().begin(); j != a->Edges().end(); ++j) {
      // Edge source
      Write<uint32>(file, a->GetId());
      // Edge destination
      Write<uint32>(file, *j);
      // Edge duration.
      Write<uint32>(file, -1);
    }
  }

  // Write actions.
  for (ActionsMapType::const_iterator i = log->actions_.begin(); i != log->actions_.end(); ++i) {
    EventAction *a = i->second;
    // EventAction id.
    Write<uint32>(file, a->GetId());
    // EventAction type.
    Write<uint32>(file, 0); // 0 == unknown
    // Number of operations.
    Write<uint32>(file, a->Ops().size());
    for (EventAction::OpsType::const_iterator j = a->Ops().begin(); j != a->Ops().end(); ++j) {
      // Operation type.
      Write<uint32>(file, j->GetType());
      // Memory location.
      Write<uint32>(file, j->GetLocation());
    }
  }

  // Write the sources.
  Write<uint32>(file, log->strings_[SOURCE_STRINGS].size());
  file.WriteAtCurrentPos(&log->strings_[SOURCE_STRINGS][0], log->strings_[SOURCE_STRINGS].size());
  Write<uint32>(file, log->nstrings_[SOURCE_STRINGS] * 2);

  // Write the values.
  Write<uint32>(file, log->strings_[VALUE_STRINGS].size());
  file.WriteAtCurrentPos(&log->strings_[VALUE_STRINGS][0], log->strings_[VALUE_STRINGS].size());
  Write<uint32>(file, log->nstrings_[VALUE_STRINGS] * 2);
}

} // end namespace

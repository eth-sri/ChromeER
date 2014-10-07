#ifndef CONTENT_BROWSER_EVENTRACER_EVENT_RACER_LOG_HOST_H_
#define CONTENT_BROWSER_EVENTRACER_EVENT_RACER_LOG_HOST_H_

#include "base/basictypes.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "content/public/common/eventracer.h"
#include "ipc/ipc_listener.h"

namespace content {

namespace detail {

class Operation {
public:
  enum Type {
    ENTER_SCOPE,
    EXIT_SCOPE,
    READ_MEMORY,
    WRITE_MEMORY,
    TRIGGER_ARC,
    MEMORY_VALUE,
    OTHER
  };

  Operation(Type type, size_t loc = 0) : type_(type), loc_(loc) {}

  Type GetType() const { return type_; }
  const char *GetTypeStr() const;

  size_t GetLocation() const { return loc_ ;}

private:
  Type type_;
  size_t loc_;
  static const char *typestr[];
};

class EventAction {
public:
  EventAction(unsigned int);
  ~EventAction();

  unsigned int GetId() { return id_; }

  void AddEdge(unsigned int dst);

  typedef std::vector<unsigned int> EdgesType;
  EdgesType       &Edges()       { return edges_; }
  const EdgesType &Edges() const { return edges_; }

  void AddOperation(unsigned int, size_t);

  typedef std::vector<Operation> OpsType;
  OpsType       &Ops()       { return ops_; }
  const OpsType &Ops() const { return ops_; }

private:
  unsigned int id_;
  EdgesType edges_;
  OpsType ops_;
};

} // end namespace detail

class EventRacerLogHost : public IPC::Listener {
public:
  EventRacerLogHost(int32);
  virtual ~EventRacerLogHost();

  typedef detail::EventAction EventAction;
  typedef detail::Operation Operation;

  uint32 GetId() const { return id_; }

  int32 GetRoutingId() { return routing_id_; }

  EventAction *CreateEventAction(unsigned int);
  void CreateEdge(unsigned int, unsigned int);
  void UpdateStringTable(size_t, const std::vector<std::string> &);

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  static void WriteDot(scoped_ptr<EventRacerLogHost> log, int32 site_id);
  static void WriteBin(scoped_ptr<EventRacerLogHost> log, int32 site_id);

private:
  static uint32 next_event_racer_log_id_;
  uint32 id_;
  int32 routing_id_;
  uint32 nedges_;

  typedef base::ScopedPtrHashMap<unsigned int, EventAction> ActionsMapType;
  ActionsMapType actions_;
  size_t nstrings_;
  std::vector<char> strings_;

  void OnCompletedEventAction(const blink::WebEventAction &);
  void OnHappensBefore(const std::vector<blink::WebEventActionEdge> &);
  void OnUpdateStringTable(size_t, const std::vector<std::string> &);
};

} // namespace content

#endif // CONTENT_BROWSER_EVENTRACER_EVENT_RACER_LOG_HOST_H_

#ifndef CONTENT_BROWSER_EVENTRACER_EVENT_RACER_LOG_HOST_H_
#define CONTENT_BROWSER_EVENTRACER_EVENT_RACER_LOG_HOST_H_

#include "base/basictypes.h"
#include "base/containers/scoped_ptr_hash_map.h"

namespace content {

class EventAction {
public:
 EventAction(unsigned int id) : id_ (id) {}

  unsigned int GetId() { return id_; }

  void AddEdge(unsigned int dst);

  typedef std::vector<unsigned int>::iterator iterator;
  typedef std::vector<unsigned int>::const_iterator const_iterator;

  iterator begin() { return edges_.begin(); }
  const_iterator begin() const { return edges_.begin(); }

  iterator end() { return edges_.end(); }
  const_iterator end() const { return edges_.end(); }

private:
  unsigned int id_;
  std::vector<unsigned int> edges_;
};

class EventRacerLogHost {
public:
  EventRacerLogHost();
  uint32 GetId() const { return id_; }

  EventAction *CreateEventAction(unsigned int);
  void CreateEdge(unsigned int, unsigned int);

  static void WriteDot(scoped_ptr<EventRacerLogHost> log, int32 site_id);

private:
  static uint32 next_event_racer_log_id_;
  uint32 id_;

  typedef base::ScopedPtrHashMap<unsigned int, EventAction> ActionsMapType;
  ActionsMapType actions_;
};

} // namespace content

#endif // CONTENT_BROWSER_EVENTRACER_EVENT_RACER_LOG_HOST_H_

#ifndef CONTENT_BROWSER_EVENTRACER_EVENT_RACER_LOG_HOST_H_
#define CONTENT_BROWSER_EVENTRACER_EVENT_RACER_LOG_HOST_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace content {

class EventRacerLogHost {
public:
  EventRacerLogHost();
  uint32 GetId() const { return id_; }

  static void Write(scoped_ptr<EventRacerLogHost> log, int32 site_id);

private:
  static uint32 next_event_racer_log_id_;
  uint32 id_;
};

} // namespace content

#endif // CONTENT_BROWSER_EVENTRACER_EVENT_RACER_LOG_HOST_H_

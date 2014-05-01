#include "event_racer_log_host.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"

namespace content {

uint32 EventRacerLogHost::next_event_racer_log_id_ = 1;

EventRacerLogHost::EventRacerLogHost()
 : id_ (next_event_racer_log_id_++) {
}

void EventRacerLogHost::Write(scoped_ptr<EventRacerLogHost> log, int32 site_id) {
  base::FilePath path(
    base::StringPrintf("eventracer--site-%u--id-%u.log", site_id, log->GetId()));
  base::File file(path, base::File::FLAG_WRITE | base::File::FLAG_CREATE_ALWAYS);
  const char data[] = "alabala";
  file.WriteAtCurrentPos(data, sizeof data - 1);
}

} // end namespace

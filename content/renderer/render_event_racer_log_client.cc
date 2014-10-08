#include "render_event_racer_log_client.h"
#include "content/common/event_racer_messages.h"
#include "content/public/renderer/render_thread.h"

namespace content {

RenderEventRacerLogClient::RenderEventRacerLogClient(int32 rid)
    : routing_id_(rid) {
}

RenderEventRacerLogClient::~RenderEventRacerLogClient() {}

void RenderEventRacerLogClient::didCompleteEventAction(const WebEventAction &a) {
  Send(new EventRacerLogHostMsg_CompletedEventAction(routing_id_, a));
}

void RenderEventRacerLogClient::didHappenBefore(const WebVector<WebEventActionEdge> &wv) {
  std::vector<blink::WebEventActionEdge> v;
  v.reserve(wv.size());
  for (size_t i = 0; i < wv.size(); ++i)
    v.push_back(wv[i]);
  Send(new EventRacerLogHostMsg_HappensBefore(routing_id_, v));
}

void RenderEventRacerLogClient::didUpdateStringTable(size_t kind, const WebVector<WebString> &wv) {
  std::vector<std::string> v;
  for (size_t i = 0; i < wv.size(); ++i)
    v.push_back (wv[i].utf8());
  Send(new EventRacerLogHostMsg_UpdateStringTable(routing_id_, kind, v));
}

bool RenderEventRacerLogClient::Send(IPC::Message *msg) {
  return RenderThread::Get()->Send(msg);
}

} // end namespace content

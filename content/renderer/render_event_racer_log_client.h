#ifndef CONTENT_RENDERER_RENDER_EVENT_RACER_LOG_CLIENT_H_
#define CONTENT_RENDERER_RENDER_EVENT_RACER_LOG_CLIENT_H_

#include "ipc/ipc_sender.h"
#include "third_party/WebKit/public/web/WebEventRacer.h"

namespace content {

using blink::WebEventAction;
using blink::WebEventActionEdge;
using blink::WebVector;
using blink::WebString;

class RenderEventRacerLogClient
    : public IPC::Sender,
      NON_EXPORTED_BASE(public blink::WebEventRacerLogClient) {
public:
  RenderEventRacerLogClient(int32);
  virtual ~RenderEventRacerLogClient();

  virtual void didCompleteEventAction(const WebEventAction &) OVERRIDE;
  virtual void didHappenBefore(const WebVector<WebEventActionEdge> &) OVERRIDE;
  virtual void didUpdateStringTable(size_t, const WebVector<WebString> &) OVERRIDE;

  virtual bool Send(IPC::Message* msg) OVERRIDE;

private:
  int32 routing_id_;
};

} // end namespace content

#endif // CONTENT_RENDERER_RENDER_EVENT_RACER_LOG_CLIENT_H_

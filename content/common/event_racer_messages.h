#include "content/public/common/eventracer.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START EventRacerMsgStart

// -----------------------------------------------------------------------------
// EventRacer messages
IPC_STRUCT_TRAITS_BEGIN(blink::WebOperation)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(location)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebEventAction)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(ops)
IPC_STRUCT_TRAITS_END()

// Completed an EventRacer action.
IPC_MESSAGE_ROUTED1(EventRacerLogHostMsg_CompletedEventAction, blink::WebEventAction)

// HappensBefore edges
IPC_MESSAGE_ROUTED1(EventRacerLogHostMsg_HappensBefore,
                    std::vector<blink::WebEventActionEdge>)

IPC_MESSAGE_ROUTED2(EventRacerLogHostMsg_UpdateStringTable, size_t, std::vector<std::string>)

/* UEFI event and callback wrapper
 */
#include <linux/kernel.h>
#include "efiwrapper.h"

typedef
VOID
(EFIAPI *EFI_EVENT_NOTIFY) (
    IN EFI_EVENT                Event,
    IN VOID                     *Context
    );

typedef UINTN EFI_TPL;
#define TPL_APPLICATION       4
#define TPL_CALLBACK          8
#define TPL_NOTIFY           16
#define TPL_HIGH_LEVEL       31
#define EFI_TPL_APPLICATION  TPL_APPLICATION
#define EFI_TPL_CALLBACK     TPL_CALLBACK
#define EFI_TPL_NOTIFY       TPL_NOTIFY
#define EFI_TPL_HIGH_LEVEL   TPL_HIGH_LEVEL

#define EVT_TIMER                           0x80000000
#define EVT_RUNTIME                         0x40000000
#define EVT_RUNTIME_CONTEXT                 0x20000000

#define EVT_NOTIFY_WAIT                     0x00000100
#define EVT_NOTIFY_SIGNAL                   0x00000200

#define EVT_SIGNAL_EXIT_BOOT_SERVICES       0x00000201
#define EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE   0x60000202

#define EVT_EFI_SIGNAL_MASK                 0x000000FF
#define EVT_EFI_SIGNAL_MAX                  4

#define EFI_EVENT_TIMER                         EVT_TIMER
#define EFI_EVENT_RUNTIME                       EVT_RUNTIME
#define EFI_EVENT_RUNTIME_CONTEXT               EVT_RUNTIME_CONTEXT
#define EFI_EVENT_NOTIFY_WAIT                   EVT_NOTIFY_WAIT
#define EFI_EVENT_NOTIFY_SIGNAL                 EVT_NOTIFY_SIGNAL
#define EFI_EVENT_SIGNAL_EXIT_BOOT_SERVICES     EVT_SIGNAL_EXIT_BOOT_SERVICES
#define EFI_EVENT_SIGNAL_VIRTUAL_ADDRESS_CHANGE EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE
#define EFI_EVENT_EFI_SIGNAL_MASK               EVT_EFI_SIGNAL_MASK
#define EFI_EVENT_EFI_SIGNAL_MAX                EVT_EFI_SIGNAL_MAX


typedef
EFI_STATUS
(EFIAPI *EFI_CREATE_EVENT) (
    IN UINT32                       Type,
    IN EFI_TPL                      NotifyTpl,
    IN EFI_EVENT_NOTIFY             NotifyFunction,
    IN VOID                         *NotifyContext,
    OUT EFI_EVENT                   *Event
    );

typedef
EFI_STATUS 
(EFIAPI *EFI_REGISTER_PROTOCOL_NOTIFY) (
    IN EFI_GUID                 *Protocol,
    IN EFI_EVENT                Event,
    OUT VOID                    **Registration
    );

typedef
EFI_STATUS
(EFIAPI *EFI_SIGNAL_EVENT) (
    IN EFI_EVENT                Event
    );

typedef struct {
	EFI_EVENT event;
	void * registration;
	void * context;
	void (*handler)(void *);
} uefi_event_t;


// UEFI calls back with MS ABI, so this must translate to Linux ABI
static void EFIAPI uefi_event_callback(EFI_EVENT Event, VOID * context)
{
	uefi_event_t * ev = context;
	ev->handler(ev->context);
}

int uefi_register_protocol_callback(
	EFI_GUID * guid,
	void (*handler)(void*),
	void * context
)
{
	EFI_CREATE_EVENT create_event = (void*) gBS->create_event;
	EFI_REGISTER_PROTOCOL_NOTIFY register_protocol_notify = (void*) gBS->register_protocol_notify;
	EFI_SIGNAL_EVENT signal_event = (void*) gBS->signal_event;
	uefi_event_t * ev = kzalloc(sizeof(*ev), GFP_KERNEL);
	int status;

	ev->handler = handler;
	ev->context = context;

	status = create_event(
		EVT_NOTIFY_SIGNAL,
		TPL_CALLBACK,
		uefi_event_callback,
		ev,
		&ev->event
	);
	printk("create event %d\n", status);

	status = register_protocol_notify(
		guid,
		ev->event,
		&ev->registration
	);
	printk("register protocol %d\n", status);

	status = signal_event(ev->event);
	printk("signal event %d\n", status);

	return 0;
}


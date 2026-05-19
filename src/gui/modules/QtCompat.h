#pragma once

// Windows headers can define a `connect` macro that breaks Qt signal/slot connections.
#ifdef connect
#undef connect
#endif

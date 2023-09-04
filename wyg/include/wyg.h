/**
 * @file wyg.h
 */

// ================================================================================================================================

typedef struct wyg_context_t wyg_context_t;

typedef void* wyg_window_t;

// ================================================================================================================================

#ifdef __cplusplus
extern "C" {
#endif

extern wyg_context_t* wyg_create_context(wyg_window_t window);

extern void wyg_make_current(wyg_context_t* context);

extern void wyg_destroy_context(wyg_context_t* context);

extern void wyg_swap_buffers(void);

#ifdef __cplusplus
}
#endif

// ================================================================================================================================

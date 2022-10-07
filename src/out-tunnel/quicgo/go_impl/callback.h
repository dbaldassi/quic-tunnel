#ifndef CALLBACK_H
#define CALLBACK_H

#ifdef __cplusplus
extern "C" {
#endif

	typedef void* wrapper_t;

	void on_message_received(wrapper_t*, const char*, int);
	void on_qlog_filename(wrapper_t*, const char*, int);
	
#ifdef __cplusplus
}
#endif
	

#endif /* CALLBACK_H */

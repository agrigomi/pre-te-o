#include "iScheduler.h"
#include "iCPUHost.h"
#include "repository.h"

static iCPUHost *_g_p_cpu_host_ = 0;

iCPUHost *get_cpu_host(void) {
	if(__g_p_repository__) {
		if(!_g_p_cpu_host_) 
			_g_p_cpu_host_ = (iCPUHost *)__g_p_repository__->get_object_by_interface(I_CPU_HOST, RY_STANDALONE);
	}
	
	return _g_p_cpu_host_;
}
_u16 cpu_count(void) {
	_u16 r = 0;
	iCPUHost *p_cpu_host = get_cpu_host();
	if(p_cpu_host)
		r = p_cpu_host->get_cpu_count();
	return r;
}
// get current CPU
iCPU *get_cpu(void) {
	iCPU *r = 0;
	iCPUHost *p_cpu_host = get_cpu_host();
	if(p_cpu_host)
		r = p_cpu_host->get_cpu();
	return r;
}
// get CPU(cpu_id)
iCPU *get_cpu(_u16 cpu_id) {
	iCPU *r = 0;
	iCPUHost *p_cpu_host = get_cpu_host();
	if(p_cpu_host)
		r = p_cpu_host->get_cpu(cpu_id);
	return r;
}
iCPU *get_cpu_by_index(_u16 idx) {
	iCPU *r = 0;
	iCPUHost *p_cpu_host = get_cpu_host();
	if(p_cpu_host)
		r = p_cpu_host->get_cpu_by_index(idx);
	return r;
}
// get current CPU id
_u16 cpu_id() {
	_u16 r = 0;
	iCPU *p_cpu = get_cpu();
	if(p_cpu)
		r = p_cpu->cpu_id();
	return r;
}
// enable/disable IRQ-s
bool enable_interrupts(bool enable) {
	bool r = true;
	iCPU *p_cpu = get_cpu();
	if(p_cpu)
		r = p_cpu->enable_interrupts(enable);
	return r;
}
// get current CPU blender
iBlender *get_blender(void) {
	iBlender *r = 0;
	iCPU *p_cpu = get_cpu();
	if(p_cpu)
		r = p_cpu->get_blender();
	return r;
}
// get blender of CPU(cpu_id)
iBlender *get_blender(_u16 cpu_id) {
	iBlender *r = 0;
	iCPU *p_cpu = get_cpu(cpu_id);
	if(p_cpu)
		r = p_cpu->get_blender();
	return r;
}
// create system thread on current CPU
_u32 create_systhread(_thread_t *p_entry, _u32 stack_sz, void *p_data, bool suspend, _u32 resolution) {
	_u32 r = 0;
	iBlender *p_sh = get_blender();
	if(p_sh)
		r = p_sh->create_systhread(p_entry, stack_sz, p_data, suspend, resolution);
	return r;
}
// create system thread on CPU(cpu_id)
_u32 create_systhread(_u16 cpu_id, _thread_t *p_entry, _u32 stack_sz, void *p_data, bool suspend, _u32 resolution) {
	_u32 r = 0;
	iBlender *p_sh = get_blender(cpu_id);
	if(p_sh)
		r = p_sh->create_systhread(p_entry, stack_sz, p_data, suspend, resolution);
	return r;
}
// sleep current systhread
void sleep_systhread(_u32 tm_ms) {
	iBlender *p_sh = get_blender();
	if(p_sh)
		p_sh->sleep_systhread(tm_ms);
}
// suspend systhread(thread_id)
void suspend_systhread(_u32 thread_id) {
	iBlender *p_sh = get_blender();
	if(p_sh)
		p_sh->suspend_systhread(thread_id);
}
// resume systhread(thread_id)
void resume_systhread(_u32 thread_id) {
	iBlender *p_sh = get_blender();
	if(p_sh)
		p_sh->resume_systhread(thread_id);
}
void switch_context(void) {
	iCPU *p_cpu = get_cpu();
	if(p_cpu)
		p_cpu->switch_context();
}
_u32 wait_event(iEvent *p_event, _u32 mask, _u32 timeout) {
	_u32 r = 0;
	if(p_event) {
		iBlender *p_sh = get_blender();
		if(p_sh && p_sh->is_running())
			r = p_sh->wait_event(p_event, mask, timeout);
		else {
			iCPU *p_cpu = get_cpu();
			if(p_cpu) {
				_u32  tmo = (timeout != EVENT_TIMEOUT_INFINITE)?timeout*1000:timeout;
				while(!r && tmo) {
					bool irqs = p_cpu->enable_interrupts(false);
					r = p_event->check(mask);
					p_cpu->enable_interrupts(irqs);
					if(tmo != EVENT_TIMEOUT_INFINITE)
						tmo--;
				}
			} else
				r = p_event->wait(mask, timeout);
		}
	}
	return r;
}
HMUTEX acquire_mutex(iMutex *p_mutex, HMUTEX hlock, _u32 timeout) {
	HMUTEX r = 0;
	if(p_mutex) {
		iBlender *p_sh = get_blender();
		if(p_sh && p_sh->is_running())
			r = p_sh->acquire_mutex(p_mutex, hlock, timeout);
		else {
			iCPU *p_cpu = get_cpu();
			if(p_cpu) {
				_u32  tmo = (timeout != MUTEX_TIMEOUT_INFINITE)?timeout*1000:timeout;
				while(!r && tmo) {
					bool irqs = p_cpu->enable_interrupts(false);
					r = p_mutex->try_lock(hlock);
					p_cpu->enable_interrupts(irqs);
					if(tmo != MUTEX_TIMEOUT_INFINITE)
						tmo--;
				}
			} else
				r = p_mutex->lock(hlock, timeout);
		}
	}
	return r;
}

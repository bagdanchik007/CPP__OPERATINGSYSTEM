#include "Schedule.h"
#include "vmm.h"




namespace {
	Task* currentTask = nullptr;// TASK,der gerade auf der GPU läuft
	Task* ready_head = nullptr;// Kopf der ready queue
	Task* ready_tail = nullptr;// Ende der ready queue
}

extern"C" void scheduler_init() {
	current_task = nullptr;
	ready_head = nullptr;
	ready_head = nullptr;
}

extern "C" void scheduler_add_task(Task task) {
	task->state = TaskState::READY;
	task->next = nullptr;

	if (!ready_head) {
	//Queue war leer, setze Kopf und Schwanz auf die neue Aufgabe
		ready_head = task;
		ready_tail = task;
	
	}
	else {
		//Füge die neue Aufgabe am Ende der Warteschlange hinzu
		ready_tail->next = task;
		ready_tail = task;
	}
}

extern "C" Task* scheduler_current_task() {
	return current_task;
}

extern "C" void scheduler_tick() {

	if (!current_task) {
		if (!ready_head) return; // Keine Aufgabe zum Ausführen
		current_task = ready_head;
		ready_head = ready_head->next;
		if (!ready_head) ready_tail = nullptr; // Warteschlange ist jetzt leer
		current_task->state = TaskState::RUNNING;
		return;//Hinweis; Der allererste Task wird gestartet, wenn der Scheduler zum ersten Mal getickt wird
		//separat über die Funktion scheduler_start() gestartet
   }
	//Kein anderer Task wartet --> aktueller Task läuft weiter
	if (!ready_head) return;

	//TASK ANS ENDE DER QUEUE HÄNGEN
	Task* prev_task = current_task;
	if (prev_task->state == TaskState::RUNNING) {
		prev_task->state = TaskState::READY;
		prev_task->next = nullptr;
		if(!ready_head) {
			ready_head = prev_task;
			ready_tail = prev_task;
		}
		else {
			ready_tail->next = prev_task;
			ready_tail = prev_task;
		}
	}
	//Nächster Task aus der Warteschlange
	Task* next_task = ready_head;
	ready_head = ready_head->next;
	if (!ready_head) ready_tail = nullptr; // Warteschlange ist jetzt leer

	next_task->state = TaskState::RUNNING;
	current_task = next_task;

	//Adressraumwechsel auf den neuen Task
	// (0 = teilt sich den Adressraum mit dem Kernel, 1 = eigener Adressraum)
	if(next_task->pm4_phys != 0) {
		vmm_switch(next_task->pm4_phys);
	}
	//Der eigentliche Registerwechsel auf den neuen Task wird in der Funktion scheduler_switch() durchgeführt, die in der Datei scr/arch/amd64/asm.S implementiert ist.
	context_switch(prev_task, next_task);

	//Wichtig: Die Funktion context_switch() kehrt nicht zurück, da sie den Kontext des aktuellen Tasks speichert und den Kontext des nächsten Tasks wiederherstellt. Daher wird der Code nach dem Aufruf von context_switch() nicht mehr ausgeführt.
	// die Ausführung des aktuellen Tasks wird unterbrochen und der nächste Task wird fortgesetzt.
	// zurück dessen Zieladresse beim leztem Switsch GESICHERT WIRD, also der nächste Task wird fortgesetzt, als ob er nie unterbrochen worden wäre.
}
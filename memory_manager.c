#include <linux/init.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

MODULE_LICENSE("GPL");

static int pid;

module_param(pid, int, 0);

struct task_struct* task;
struct mm_struct* mm;
struct vm_area_struct * vma;

static int vmaCount = 0;

// used to walk through the page table
pgd_t* pgd;
p4d_t* p4d;
pud_t* pud;
pmd_t* pmd;
pte_t* ptep, pte;

// for use in the while loop
unsigned long startAddr;
unsigned long endAddr;
unsigned long pageAddr;
unsigned long ii;
//#define TIMER_INTERVAL_NS (10 * NSEC_PER_SEC)
unsigned long timer_interval_ns = 10e9; // 10-second timer
//unsigned long timer_interval_ns = 10;

// timer object
static struct hrtimer timerRestart, timerNoRestart;
ktime_t currtime , interval, noInterval;

unsigned long workingCount = 0;
unsigned long residentCount = 0;
unsigned long swapCount = 0;

static int jj = 0;

enum hrtimer_restart callback_restart(struct hrtimer* test) {
	//printk("inside the callback restart function");
	//if (jj < 3) {
		task = pid_task(find_vpid(pid), PIDTYPE_PID);
		mm = task->mm;
		if (mm == NULL) {
			//printk("mm value is null");
		} else {
			vma = mm->mmap;
			while (vma != NULL) {
				//printk("VMA #%d about to be walked", vmaCount);
				for (ii = vma->vm_start; ii < vma->vm_end; ii += PAGE_SIZE) {
				// do the page walk
					pgd = pgd_offset(mm, ii);
					p4d = p4d_offset(pgd, ii);
					if (p4d_none(*p4d) || p4d_bad(*p4d)) {
						continue;
					}
	
					pud = pud_offset(p4d, ii);
					if (pud_none(*pud) || pud_bad(*pud)) {
						continue;
					}
	
					pmd = pmd_offset(pud, ii);
					if (pmd_none(*pmd) || pmd_bad(*pmd)) {
						continue;
					}
	
					ptep = pte_offset_map(pmd, ii);
					if (!ptep) {
						continue;
					}
					pte = *ptep;
					// now check for pte
					if (pte_present(pte)) {
						residentCount += 4;
						// check if valid

						if (ptep_test_and_clear_young(vma, ii, ptep) == 1) {
							workingCount += 4;
						}
					} else {
						swapCount += 4;
					}
				}
				vmaCount++;
				vma = vma->vm_next;
			}
		}
	
	//}
	
	currtime = ktime_get();
	interval = ktime_set(0, timer_interval_ns);
	hrtimer_forward(test, currtime, interval);
	//printk("PID %d : RSS = %lu KB, SWAP = %lu KB, WSS = %lu KB", pid, (residentCount/1024), (swapCount/1024), (workingCount/1024));
	printk("PID %d : RSS = %lu KB, SWAP = %lu KB, WSS = %lu KB", pid, residentCount, swapCount, workingCount);
	residentCount = 0;
	swapCount = 0;
	workingCount = 0;
	jj++;
     	return HRTIMER_RESTART;
	
}

enum hrtimer_restart callback_norestart(struct hrtimer* test) {
	//printk("inside the callback NO restart function");
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
	mm = task->mm;
	if (mm == NULL) {
		//printk("mm value is null");
	} else {
		vma = mm->mmap;
		while (vma != NULL) {
			//printk("VMA #%d about to be walked", vmaCount);
			for (ii = vma->vm_start; ii < vma->vm_end; ii += PAGE_SIZE) {
				// do the page walk
				pgd = pgd_offset(mm, ii);
				p4d = p4d_offset(pgd, ii);
				if (p4d_none(*p4d) || p4d_bad(*p4d)) {
					continue;
				}

				pud = pud_offset(p4d, ii);
				if (pud_none(*pud) || pud_bad(*pud)) {
					continue;
				}

				pmd = pmd_offset(pud, ii);
				if (pmd_none(*pmd) || pmd_bad(*pmd)) {
					continue;
				}

				ptep = pte_offset_map(pmd, ii);
				if (!ptep) {
					continue;
				}
				pte = *ptep;
				// now check for pte
				if (pte_present(pte)) {
					residentCount += 4;

					// check if valid
					if (ptep_test_and_clear_young(vma, ii, ptep) == 1) {
						workingCount += 4;
					}
				}
				else {
					swapCount += 4;
				}
			
			}
			vmaCount++;
			vma = vma->vm_next;
		}
	}
	
	//printk("PID %d : RSS = %lu KB, SWAP = %lu KB, WSS = %lu KB", pid, (residentCount/1024), (swapCount/1024), (workingCount/1024));
	printk("PID %d : RSS = %lu KB, SWAP = %lu KB, WSS = %lu KB", pid, residentCount, swapCount, workingCount);
	residentCount = 0;
	swapCount = 0;
	workingCount = 0;
     	return HRTIMER_NORESTART;
	
}


static void timer_begin(void) {
// for first two test cases
	interval = ktime_set(0, timer_interval_ns);
	hrtimer_init(&timerRestart, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timerRestart.function = &callback_restart;
	hrtimer_start(&timerRestart, interval, HRTIMER_MODE_REL);
	//hrtimer_start(&timerRestart, ns_to_ktime(timer_interval_ns), HRTIMER_MODE_REL);
	
// for last test case
	//noInterval = ktime_set(0, timer_interval_ns);
	//hrtimer_init(&timerNoRestart, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	//timerNoRestart.function = &callback_norestart;
	//hrtimer_start(&timerNoRestart, noInterval, HRTIMER_MODE_REL);
}

int ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr, pte_t *ptep) {
	int ret = 0;
	if (pte_young(*ptep)) {
		ret = test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *) &ptep->pte);
	}
	return ret;
}

static int __init initial(void) {
	printk("inserting the module for project 3");
	// start timer call to get them initialized
	timer_begin();
	//printk("timer is done");
	return(0);

}

static void __exit exit_function(void) {
	printk("removing the module for project 3\n");
	hrtimer_cancel(&timerRestart);
	//hrtimer_cancel(&timerNoRestart);
	//printk("module removed\n");
}

module_init(initial);
module_exit(exit_function);
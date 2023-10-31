/****************************************************************************
 *
 *   Copyright (c) 2021 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include "arp.hpp"


static void ping_result(FAR const struct ping_result_s *result)
{
	switch (result->code) {
	case ICMP_E_HOSTIP:
		// fprintf(stderr, "ERROR: ping_gethostip(%s) failed\n",
		// 	result->info->hostname);
		break;

	case ICMP_E_MEMORY:
		// fprintf(stderr, "ERROR: Failed to allocate memory\n");
		break;

	case ICMP_E_SOCKET:
		// fprintf(stderr, "ERROR: socket() failed: %d\n", result->extra);
		break;

	case ICMP_I_BEGIN:
		// printf("PING %u.%u.%u.%u %u bytes of data\n",
		//        (result->dest.s_addr) & 0xff,
		//        (result->dest.s_addr >> 8) & 0xff,
		//        (result->dest.s_addr >> 16) & 0xff,
		//        (result->dest.s_addr >> 24) & 0xff,
		//        result->info->datalen);
		break;

	case ICMP_E_SENDTO:
		// fprintf(stderr, "ERROR: sendto failed at seqno %u: %d\n",
		// 	result->seqno, result->extra);
		break;

	case ICMP_E_SENDSMALL:
		// fprintf(stderr, "ERROR: sendto returned %d, expected %u\n",
		// 	result->extra, result->outsize);
		break;

	case ICMP_E_POLL:
		// fprintf(stderr, "ERROR: poll failed: %d\n", result->extra);
		break;

	case ICMP_W_TIMEOUT:
		// printf("No response from %u.%u.%u.%u: icmp_seq=%u time=%d ms\n",
		//        (result->dest.s_addr) & 0xff,
		//        (result->dest.s_addr >> 8) & 0xff,
		//        (result->dest.s_addr >> 16) & 0xff,
		//        (result->dest.s_addr >> 24) & 0xff,
		//        result->seqno, result->extra);
		break;

	case ICMP_E_RECVFROM:
		// fprintf(stderr, "ERROR: recvfrom failed: %d\n", result->extra);
		break;

	case ICMP_E_RECVSMALL:
		// fprintf(stderr, "ERROR: short ICMP packet: %d\n", result->extra);
		break;

	case ICMP_W_IDDIFF:
		// fprintf(stderr,
		// 	"WARNING: Ignoring ICMP reply with ID %d.  "
		// 	"Expected %u\n",
		// 	result->extra, result->id);
		break;

	case ICMP_W_SEQNOBIG:
		// fprintf(stderr,
		// 	"WARNING: Ignoring ICMP reply to sequence %d.  "
		// 	"Expected <= %u\n",
		// 	result->extra, result->seqno);
		break;

	case ICMP_W_SEQNOSMALL:
		// fprintf(stderr, "WARNING: Received after timeout\n");
		break;

	case ICMP_I_ROUNDTRIP:
		// printf("%u bytes from %u.%u.%u.%u: icmp_seq=%u time=%d ms\n",
		//        result->info->datalen,
		//        (result->dest.s_addr) & 0xff,
		//        (result->dest.s_addr >> 8) & 0xff,
		//        (result->dest.s_addr >> 16) & 0xff,
		//        (result->dest.s_addr >> 24) & 0xff,
		//        result->seqno, result->extra);
		break;

	case ICMP_W_RECVBIG:
		// fprintf(stderr,
		// 	"WARNING: Ignoring ICMP reply with different payload "
		// 	"size: %d vs %u\n",
		// 	result->extra, result->outsize);
		break;

	case ICMP_W_DATADIFF:
		// fprintf(stderr, "WARNING: Echoed data corrupted\n");
		break;

	case ICMP_W_TYPE:
		// fprintf(stderr, "WARNING: ICMP packet with unknown type: %d\n",
		// 	result->extra);
		break;

	case ICMP_I_FINISH:
		if (result->nrequests > 0) {
			unsigned int tmp;

			/* Calculate the percentage of lost packets */

			tmp = (100 * (result->nrequests - result->nreplies) +
			       (result->nrequests >> 1)) /
			      result->nrequests;

			printf("%u packets transmitted, %u received, %u%% packet loss, time %ld ms\n",
			       result->nrequests, result->nreplies, tmp, result->extra);
		}

		break;
	}
}
ARP::ARP() :
	ModuleParams(nullptr),
	ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::net_default)
{
	info.count     = 1;
	info.datalen   = 56;
	info.delay     = 3;
	info.timeout   = 10;
	info.hostname  = "192.168.144.1";
	info.callback  = ping_result;
	inaddr.sin_family      = AF_INET;
	inaddr.sin_port        = 0;
	inaddr.sin_addr.s_addr = inet_addr("192.168.144.1");
}

bool ARP::init()
{
	ScheduleOnInterval(INTERVAL_US);
	return true;
}

void ARP::Run()
{
	if (should_exit()) {
		ScheduleClear();
		exit_and_cleanup();
		return;
	}


	if (netlib_getifstatus("eth0", &if_flags) == 0) {
		if (if_flags & IFF_UP) {

			ret = netlib_get_arpmapping(&inaddr, mac.ether_addr_octet);

			if (ret < 0) {
				// not get arp hw
				icmp_ping(&info);

			} else {
				// do nothing
			}
		} else {
			ret  = netlib_ifup("eth0");
			PX4_INFO("retry ifup eth0...%s\n", (ret == OK) ? "OK" : "Failed");
		}

	} else {
		PX4_INFO("Failed to retrieve interface status");
	}








}

int ARP::task_spawn(int argc, char *argv[])
{
	ARP *instance = new ARP();

	if (instance) {
		_object.store(instance);
		_task_id = task_id_is_work_queue;

		if (instance->init()) {
			return PX4_OK;
		}

	} else {
		PX4_ERR("alloc failed");
	}

	delete instance;
	_object.store(nullptr);
	_task_id = -1;

	return PX4_ERROR;
}

int ARP::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}

int ARP::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description

)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("arp", "find");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();
	return 0;
}

extern "C" __EXPORT int arp_find_main(int argc, char *argv[])
{
	return ARP::main(argc, argv);
}

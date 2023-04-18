#include "fd_frank.h"
#include "../../ballet/pack/fd_pack.h"

#if FD_HAS_FRANK

int
fd_frank_pack_task( int     argc,
                    char ** argv ) {
  (void)argc;
  fd_log_thread_set( argv[0] );
  FD_LOG_INFO(( "pack init" ));

  /* Parse "command line" arguments */

  char const * pod_gaddr = argv[1];
  char const * cfg_path  = argv[2];

  /* Load up the configuration for this frank instance */

  FD_LOG_INFO(( "using configuration in pod %s at path %s", pod_gaddr, cfg_path ));
  uchar const * pod     = fd_wksp_pod_attach( pod_gaddr );
  uchar const * cfg_pod = fd_pod_query_subpod( pod, cfg_path );
  if( FD_UNLIKELY( !cfg_pod ) ) FD_LOG_ERR(( "path not found" ));

  /* Join the IPC objects needed this tile instance */

  FD_LOG_INFO(( "joining %s.pack.cnc", cfg_path ));
  fd_cnc_t * cnc = fd_cnc_join( fd_wksp_pod_map( cfg_pod, "pack.cnc" ) );
  if( FD_UNLIKELY( !cnc ) ) FD_LOG_ERR(( "fd_cnc_join failed" ));
  if( FD_UNLIKELY( fd_cnc_signal_query( cnc )!=FD_CNC_SIGNAL_BOOT ) ) FD_LOG_ERR(( "cnc not in boot state" ));
  /* FIXME: CNC DIAG REGION? */

  FD_LOG_INFO(( "joining %s.dedup.mcache", cfg_path ));
  fd_frag_meta_t const * mcache = fd_mcache_join( fd_wksp_pod_map( cfg_pod, "dedup.mcache" ) );
  if( FD_UNLIKELY( !mcache ) ) FD_LOG_ERR(( "fd_mcache_join failed" ));
  ulong         depth = fd_mcache_depth( mcache );
  ulong const * sync  = fd_mcache_seq_laddr_const( mcache );
  ulong         seq   = fd_mcache_seq_query( sync );

  fd_frag_meta_t const * mline = mcache + fd_mcache_line_idx( seq, depth );

  FD_LOG_INFO(( "joining %s.verify.*.dcache", cfg_path ));
  /* Note (chunks are referenced relative to the containing workspace
     currently and there is just one workspace).  (FIXME: VALIDATE
     COMMON WORKSPACE FOR THESE) */
  fd_wksp_t * wksp = fd_wksp_containing( mcache );
  if( FD_UNLIKELY( !wksp ) ) FD_LOG_ERR(( "fd_wksp_containing failed" ));

  FD_LOG_INFO(( "joining %s.dedup.fseq", cfg_path ));
  ulong * fseq = fd_fseq_join( fd_wksp_pod_map( cfg_pod, "dedup.fseq" ) );
  if( FD_UNLIKELY( !fseq ) ) FD_LOG_ERR(( "fd_fseq_join failed" ));
  /* Hook up to this pack's flow control diagnostics (will be stored in
     the pack's fseq) */
  ulong * fseq_diag = (ulong *)fd_fseq_app_laddr( fseq );
  if( FD_UNLIKELY( !fseq_diag ) ) FD_LOG_ERR(( "fd_cnc_app_laddr failed" ));
  FD_COMPILER_MFENCE();
  fseq_diag[ FD_FSEQ_DIAG_PUB_CNT   ] = 0UL;
  fseq_diag[ FD_FSEQ_DIAG_PUB_SZ    ] = 0UL;
  fseq_diag[ FD_FSEQ_DIAG_OVRNP_CNT ] = 0UL;
  fseq_diag[ FD_FSEQ_DIAG_OVRNR_CNT ] = 0UL;
  FD_COMPILER_MFENCE();
  ulong accum_pub_cnt   = 0UL;
  ulong accum_pub_sz    = 0UL;
  ulong accum_ovrnp_cnt = 0UL;
  ulong accum_ovrnr_cnt = 0UL;

  ulong bank_cnt = fd_pod_query_ulong( cfg_pod, "pack.bank-cnt", 0UL );
  if( FD_UNLIKELY( !bank_cnt ) ) FD_LOG_ERR(( "pack.bank-cnt unset or set to zero" ));

  ulong txnq_sz = fd_pod_query_ulong( cfg_pod, "pack.txnq-sz", 0UL );
  if( FD_UNLIKELY( !txnq_sz ) ) FD_LOG_ERR(( "pack.txnq-sz unset or set to zero" ));

  ulong cu_est_tbl_sz = fd_pod_query_ulong( cfg_pod, "pack.cu-est-tbl-sz", 0UL );
  if( FD_UNLIKELY( !cu_est_tbl_sz ) ) FD_LOG_ERR(( "pack.cu-est-tbl-sz unset or set to zero" ));

  FD_LOG_INFO(( "joining %s.pack.out-mcache", cfg_path ));
  fd_frag_meta_t * out_mcache = fd_mcache_join( fd_wksp_pod_map( cfg_pod, "pack.out-mcache" ) );
  if( FD_UNLIKELY( !out_mcache ) ) FD_LOG_ERR(( "fd_mcache_join failed" ));

  FD_LOG_INFO(( "joining %s.pack.out-dcache", cfg_path ));
  uchar * out_dcache = fd_dcache_join( fd_wksp_pod_map( cfg_pod, "pack.out-dcache" ) );
  if( FD_UNLIKELY( !out_dcache ) ) FD_LOG_ERR(( "fd_dcache_join failed" ));

  FD_LOG_INFO(( "joining %s.pack.cu-est-tbl", cfg_path ));
  void * cu_est_tbl_shmem = fd_wksp_pod_map( cfg_pod, "pack.cu-est-tbl" );
  if( FD_UNLIKELY( !cu_est_tbl_shmem ) ) FD_LOG_ERR(( "pack.cu-est-tbl unset" ));

  fd_est_tbl_t * cu_est_tbl = fd_est_tbl_join( cu_est_tbl_shmem );
  if( FD_UNLIKELY( !cu_est_tbl ) ) FD_LOG_ERR(( "fd_est_tbl_join failed" ));

  FD_LOG_INFO(( "joining %s.pack.scratch", cfg_path ));
  void * pack_scratch = fd_wksp_pod_map( cfg_pod, "pack.scratch" );
  if( FD_UNLIKELY( !pack_scratch ) ) FD_LOG_ERR(( "pack.scratch unset" ));

  /* Setup local objects used by this tile */

  long lazy = fd_pod_query_long( cfg_pod, "pack.lazy", 0L );
  FD_LOG_INFO(( "configuring flow control (%s.pack.lazy %li)", cfg_path, lazy ));
  if( lazy<=0L ) lazy = fd_tempo_lazy_default( depth );
  FD_LOG_INFO(( "using lazy %li ns", lazy ));
  ulong async_min = fd_tempo_async_min( lazy, 1UL /*event_cnt*/, (float)fd_tempo_tick_per_ns( NULL ) );
  if( FD_UNLIKELY( !async_min ) ) FD_LOG_ERR(( "bad lazy" ));

  uint seed = fd_pod_query_uint( cfg_pod, "pack.seed", (uint)fd_tile_id() ); /* use app tile_id as default */
  FD_LOG_INFO(( "creating rng (%s.pack.seed %u)", cfg_path, seed ));
  fd_rng_t _rng[ 1 ];
  fd_rng_t * rng = fd_rng_join( fd_rng_new( _rng, seed, 0UL ) );
  if( FD_UNLIKELY( !rng ) ) FD_LOG_ERR(( "fd_rng_join failed" ));


  ulong * out_sync = fd_mcache_seq_laddr( out_mcache );
  ulong   out_seq  = fd_mcache_seq_query( out_sync );

  uint cu_limit  = fd_pod_query_uint( cfg_pod, "pack.cu-limit", 12000000U );
  FD_LOG_INFO(( "packing blocks of %u CUs with a max parallelism of %lu", cu_limit, bank_cnt ));

  const ulong lamports_per_signature = 5000UL;
  const ulong block_duration_ns      = 400UL*1000UL*1000UL; /* 400ms */

  pack_scratch = fd_pack_new( pack_scratch, bank_cnt, txnq_sz, cu_limit, lamports_per_signature, rng, cu_est_tbl,
                                                                                                  out_dcache, wksp, out_mcache );
  if( FD_UNLIKELY( !pack_scratch ) ) FD_LOG_ERR(( "fd_pack_new failed"  ));
  fd_pack_t * pack = fd_pack_join( pack_scratch );
  if( FD_UNLIKELY( !pack         ) ) FD_LOG_ERR(( "fd_pack_join failed" ));

  long block_duration_ticks = (long)(fd_tempo_tick_per_ns( NULL ) * (double)block_duration_ns);
  /* Start packing */

  fd_pack_reset( pack );

  FD_LOG_INFO(( "pack run" ));
  fd_cnc_signal( cnc, FD_CNC_SIGNAL_RUN );

  long now            = fd_tickcount();
  long then           = now;            /* Do housekeeping on first iteration of run loop */
  long block_end      = now + block_duration_ticks;
  long schedule_ready = now;
  for(;;) {

    /* Do housekeeping at a low rate in the background */

    if( FD_UNLIKELY( (now-then)>=0L ) ) {

      /* Send synchronization info */
      fd_mcache_seq_update( out_sync, out_seq );

      /* Send flow control credits */
      fd_fctl_rx_cr_return( fseq, seq );

      /* Send diagnostic info */
      fd_cnc_heartbeat( cnc, now );
      FD_COMPILER_MFENCE();
      fseq_diag[ FD_FSEQ_DIAG_PUB_CNT   ] += accum_pub_cnt;
      fseq_diag[ FD_FSEQ_DIAG_PUB_SZ    ] += accum_pub_sz;
      fseq_diag[ FD_FSEQ_DIAG_OVRNP_CNT ] += accum_ovrnp_cnt;
      fseq_diag[ FD_FSEQ_DIAG_OVRNR_CNT ] += accum_ovrnr_cnt;
      FD_COMPILER_MFENCE();
      accum_pub_cnt   = 0UL;
      accum_pub_sz    = 0UL;
      accum_ovrnp_cnt = 0UL;
      accum_ovrnr_cnt = 0UL;

      /* Receive command-and-control signals */
      ulong s = fd_cnc_signal_query( cnc );
      if( FD_UNLIKELY( s!=FD_CNC_SIGNAL_RUN ) ) {
        if( FD_UNLIKELY( s!=FD_CNC_SIGNAL_HALT ) ) FD_LOG_ERR(( "Unexpected signal" ));
        break;
      }

      /* Reload housekeeping timer */
      then = now + (long)fd_tempo_async_reload( rng, async_min );
    }

    /* Are we ready to end the block? */
    if( FD_UNLIKELY( (now-block_end)>=0L ) ) {
      fd_pack_next_block( pack );
      block_end += block_duration_ticks;
      schedule_ready = now;
    }

    /* Is it time to schedule the next transaction? */
    if( FD_UNLIKELY( (now-schedule_ready)>=0L ) && FD_LIKELY( fd_pack_available_cnt( pack ) ) ) {
      fd_pack_schedule_return_t result;
      result = fd_pack_schedule_transaction( pack );

#if DETAILED_LOGGING
      ulong const * seq = (ulong const *)fd_mcache_seq_laddr_const( out_mcache );
      FD_LOG_INFO(( "Out mcache seq: %lu. Emitted cnt: %hhu", fd_mcache_seq_query( seq ), result.mcache_emitted_cnt ));
      if     ( result.status==FD_PACK_SCHEDULE_RETVAL_BANKDONE )
        FD_LOG_INFO(( "Banking thread %hhu done", result.banking_thread ));
      else if( result.status==FD_PACK_SCHEDULE_RETVAL_STALLING )
        FD_LOG_INFO(( "Banking thread %hhu stalling %u.", result.banking_thread, result.stall_duration ));
      else if( result.status==FD_PACK_SCHEDULE_RETVAL_ALLDONE )
        FD_LOG_INFO(( "Transaction not scheduled because all banking threads are done." ));
      else
        FD_LOG_INFO(( "Transaction scheduled to banking thread %hhu at time %u", result.banking_thread, result.start_time ));
#else
      (void) result;
#endif
      uint min_in_use_until = fd_pack_fully_scheduled_until( pack );
      if( FD_UNLIKELY( result.status==FD_PACK_SCHEDULE_RETVAL_ALLDONE ) )
        schedule_ready = block_end;
      else
        schedule_ready = block_end - block_duration_ticks + block_duration_ticks * min_in_use_until / cu_limit;
    }

    /* See if there are any transactions waiting to be packed */
    ulong seq_found = fd_frag_meta_seq_query( mline );
    long  diff      = fd_seq_diff( seq_found, seq );
    if( FD_UNLIKELY( diff ) ) { /* caught up or overrun, optimize for expected sequence number ready */
      if( FD_LIKELY( diff<0L ) ) { /* caught up */
        FD_SPIN_PAUSE();
        now = fd_tickcount();
        continue;
      }
      /* overrun by dedup tile ... recover */
      accum_ovrnp_cnt++;
      seq = seq_found;
      /* can keep processing from the new seq */
    }

    now = fd_tickcount();

    /* At this point, we have started receiving frag seq with details in
       mline at time now.  Speculatively processs it here. */

    /* Speculative pack operations */
    fd_txn_p_t * slot          = fd_pack_prepare_insert( pack );

    ulong         sz           = (ulong)mline->sz;
    uchar const * dcache_entry = fd_chunk_to_laddr_const( wksp, mline->chunk );
    ulong         mline_sig    = mline->sig;
    /* Assume that the dcache entry is:
         Payload ....... (payload_sz bytes)
         0 or 1 byte of padding (since alignof(fd_txn) is 2)
         fd_txn ....... (size computed by fd_txn_footprint)
         payload_sz  (2B)
      mline->sz includes all three fields and the padding */
    ulong payload_sz = *(ushort*)(dcache_entry + sz - sizeof(ushort));
    uchar    const * payload = dcache_entry;
    fd_txn_t const * txn     = (fd_txn_t const *)( dcache_entry + fd_ulong_align_up( payload_sz, 2UL ) );
    fd_memcpy( slot->payload, payload, payload_sz                                                     );

    if( FD_UNLIKELY( payload_sz>FD_MTU || !fd_txn_parse( payload, payload_sz, TXN(slot), NULL ) ) ) {
      FD_LOG_NOTICE(( "Re-parsing transaction failed. Ignoring transaction." ));
      fd_pack_cancel_insert( pack, slot );
      accum_ovrnr_cnt++;
      seq = seq_found;
      continue;
    }
    (void)txn;

    slot->payload_sz = payload_sz;
    slot->mline_sig  = mline_sig;

#if DETAILED_LOGGING
    FD_LOG_NOTICE(( "Pack got a packet. Payload size: %lu, txn footprint: %lu", payload_sz,
          fd_txn_footprint( txn->instr_cnt, txn->addr_table_lookup_cnt )
        ));
#endif

    /* Check that we weren't overrun while processing */
    seq_found = fd_frag_meta_seq_query( mline );
    if( FD_UNLIKELY( fd_seq_ne( seq_found, seq ) ) ) {
      fd_pack_cancel_insert( pack, slot );
      accum_ovrnr_cnt++;
      seq = seq_found;
      continue;
    }

    /* Non-speculative pack operations */
    accum_pub_cnt++;
    accum_pub_sz += sz;

    fd_pack_insert_transaction( pack, slot );

    /* Wind up for the next iteration */
    seq   = fd_seq_inc( seq, 1UL );
    mline = mcache + fd_mcache_line_idx( seq, depth );
  }

  /* Clean up */

  fd_cnc_signal( cnc, FD_CNC_SIGNAL_BOOT );
  FD_LOG_INFO(( "pack fini" ));
  fd_rng_delete    ( fd_rng_leave   ( rng    ) );
  fd_wksp_pod_unmap( fd_fseq_leave  ( fseq   ) );
  fd_wksp_pod_unmap( fd_mcache_leave( mcache ) );
  fd_wksp_pod_unmap( fd_cnc_leave   ( cnc    ) );
  fd_wksp_pod_detach( pod );
  return 0;
}

#else

int
fd_frank_pack_task( int     argc,
                    char ** argv ) {
  (void)argc; (void)argv;
  FD_LOG_WARNING(( "unsupported for this build target" ));
  return 1;
}

#endif

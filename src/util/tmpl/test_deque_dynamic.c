#include "../fd_util.h"
#define DEQUE_NAME deque
#define DEQUE_T int

#include "fd_deque_dynamic.c"

#define DEQUE_NAME p2deque
#define DEQUE_T int
#define DEQUE_MAX_POW2 1

#include "fd_deque_dynamic.c"

#define FOOTPRINT_SZ 1024
#define DEQUE_MAX   (6UL)
#define P2DEQUE_MAX (8UL)

uchar scratch[ FOOTPRINT_SZ ];

int
main( int     argc,
      char ** argv ) {
  fd_boot( &argc, &argv );

  if( FD_UNLIKELY( deque_footprint( DEQUE_MAX ) > FOOTPRINT_SZ ) ) { 
    FD_LOG_WARNING(( "skip: adjust footprint or max" ));
    return 0;
  }
  if( FD_UNLIKELY( p2deque_footprint( P2DEQUE_MAX ) > FOOTPRINT_SZ ) ) { 
    FD_LOG_WARNING(( "skip: adjust footprint or max" ));
    return 0;
  }

  FD_LOG_NOTICE(( "Testing non-power-of-two case" ));
  void    * shdeque = deque_new ( scratch, DEQUE_MAX ); FD_TEST( shdeque );
  deque_t * deque   = deque_join( shdeque            ); FD_TEST( deque   );

  /* Make the deque have j elements with start==i */
  for( ulong i=0UL; i<DEQUE_MAX; i++ ) {
    memset( deque, 0, (DEQUE_MAX+1UL)*sizeof(deque_t) );
    FD_TEST( deque_private_hdr_from_deque( deque )->start == i );
    for( ulong j=0UL; j<=DEQUE_MAX; j++ ) {
      /* Check correctness */
      FD_TEST( deque_cnt( deque ) == j );
      FD_TEST( deque_max( deque ) == DEQUE_MAX );
      for( ulong k=0UL; k<=DEQUE_MAX; k++ ) {
        /* If k>=i, then the (k-i) element of the deque should be in slot k.
           If k<i, then the (k+1+max-i) element should be in slot k.  */
        /* If l< j, then the lth element of the deque should have value l+1.
           if l>=j, then the lth element of the deque should be 0. */
        ulong linear_idx  = (k>=i) ? (k-i) : (k+1+DEQUE_MAX-i);
        int   correct_val = (linear_idx<j) ? (int)(linear_idx+1) : 0;
        FD_TEST( deque[k] == correct_val );
      }

      if( j<DEQUE_MAX ) deque_push( deque, (int)(j+1) );
    }
    FD_TEST( 1 == deque_pop( deque ) ); /* Advance start */
    for( ulong j=1UL; j<DEQUE_MAX; j++ ) {
      FD_TEST( (int)(DEQUE_MAX+1UL-j) == deque_pop_back( deque ) );
    }
  }
  FD_TEST( deque_leave ( deque   )==shdeque         );
  FD_TEST( deque_delete( shdeque )==(void *)scratch );
  shdeque = deque_new ( scratch, DEQUE_MAX ); FD_TEST( shdeque );
  deque   = deque_join( shdeque            ); FD_TEST( deque   );

  /* Similar, but with push_front, so end==i */
  for( ulong i=0UL; i<DEQUE_MAX; i++ ) {
    memset( deque, 0, (DEQUE_MAX+1UL)*sizeof(deque_t) );
    FD_TEST( deque_private_hdr_from_deque( deque )->end == i );
    for( ulong j=0UL; j<=DEQUE_MAX; j++ ) {
      /* Check correctness */
      FD_TEST( deque_cnt( deque ) == j );
      FD_TEST( deque_max( deque ) == DEQUE_MAX );
      for( ulong k=0UL; k<=DEQUE_MAX; k++ ) {
        /* If k<i, then, the (i-k-1)th element from the end should be in slot k.
           If k>=i, then the (max-k+i) element from the end should be in slot k.  */
        /* If l< j, then the lth element from the end of the deque should have value max-l.
           if l>=j, then the lth element from the end of the deque should be 0. */
        ulong linear_idx  = (k<i) ? (i-k-1) : (i+DEQUE_MAX-k);
        int   correct_val = (linear_idx<j) ? (int)(DEQUE_MAX-linear_idx) : 0;
        FD_TEST( deque[k] == correct_val );
      }
      if( j<DEQUE_MAX ) deque_push_front( deque, (int)(DEQUE_MAX-j) );
    }
    for( ulong j=0UL; j<DEQUE_MAX; j++ ) {
      FD_TEST( (int)(j+1UL) == deque_pop( deque ) );
    }
    deque_push( deque, 99 ); /* advance end */
    FD_TEST( 99 == deque_pop( deque ) ); /* empty it */
  }



  FD_TEST( deque_leave ( deque   )==shdeque         );
  FD_TEST( deque_delete( shdeque )==(void *)scratch );

  FD_LOG_NOTICE(( "Testing power-of-two case" ));
  shdeque = p2deque_new ( scratch, P2DEQUE_MAX ); FD_TEST( shdeque );
  deque   = p2deque_join( shdeque              ); FD_TEST( deque   );

  /* Make the deque have j elements with start==i */
  for( ulong i=0UL; i<P2DEQUE_MAX; i++ ) {
    memset( deque, 0, (P2DEQUE_MAX)*sizeof(deque_t) );
    FD_TEST( (p2deque_private_hdr_from_deque( deque )->start&(P2DEQUE_MAX-1UL)) == i );
    for( ulong j=0UL; j<=P2DEQUE_MAX; j++ ) {
      /* Check correctness */
      FD_TEST( p2deque_cnt( deque ) == j );
      FD_TEST( p2deque_max( deque ) == P2DEQUE_MAX );
      for( ulong k=0UL; k<P2DEQUE_MAX; k++ ) {
        int   correct_val = (k<j) ? (int)(k+1) : 0;
        FD_TEST( deque[(i+k)&(P2DEQUE_MAX-1UL)] == correct_val );
      }

      if( j<P2DEQUE_MAX ) p2deque_push( deque, (int)(j+1) );
    }
    FD_TEST( 1 == p2deque_pop( deque ) ); /* Advance start */
    for( ulong j=1UL; j<P2DEQUE_MAX; j++ ) {
      FD_TEST( (int)(P2DEQUE_MAX+1UL-j) == p2deque_pop_back( deque ) );
    }
  }
  FD_TEST( p2deque_leave ( deque   )==shdeque         );
  FD_TEST( p2deque_delete( shdeque )==(void *)scratch );
  shdeque = p2deque_new ( scratch, P2DEQUE_MAX ); FD_TEST( shdeque );
  deque   = p2deque_join( shdeque              ); FD_TEST( deque   );

  /* Similar, but with push_front, so end==i */
  for( ulong i=0UL; i<P2DEQUE_MAX; i++ ) {
    memset( deque, 0, (P2DEQUE_MAX)*sizeof(deque_t) );
    FD_TEST( p2deque_private_hdr_from_deque( deque )->end == i );
    for( ulong j=0UL; j<=P2DEQUE_MAX; j++ ) {
      /* Check correctness */
      FD_TEST( p2deque_cnt( deque ) == j );
      FD_TEST( p2deque_max( deque ) == P2DEQUE_MAX );
      for( ulong k=0UL; k<P2DEQUE_MAX; k++ ) { /* Checking the kth from the end */
        int   correct_val = (k<j) ? (int)(P2DEQUE_MAX-k) : 0;
        FD_TEST( deque[(i+P2DEQUE_MAX-k-1UL)&(P2DEQUE_MAX-1UL)] == correct_val );
      }
      /* Using push_front here triggers the underflow case */
      if( j<P2DEQUE_MAX ) p2deque_push_front( deque, (int)(P2DEQUE_MAX-j) );
    }
    for( ulong j=0UL; j<P2DEQUE_MAX; j++ ) {
      FD_TEST( (int)(j+1UL) == p2deque_pop( deque ) );
    }
    p2deque_push( deque, 99 ); /* advance end */
    FD_TEST( 99 == p2deque_pop( deque ) ); /* empty it */
  }



  FD_TEST( p2deque_leave ( deque   )==shdeque         );
  FD_TEST( p2deque_delete( shdeque )==(void *)scratch );

  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}
/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

/**
* Unit tests for NGS Pileup interface, CSRA1 based implementation
*/

#include <ngs/ncbi/NGS.hpp>

#include <ktst/unit_test.hpp>

#include <sysalloc.h>
#include <assert.h>
#include <memory.h> // memset

#include <sstream>

using namespace ncbi::NK;

TEST_SUITE(NgsCsra1PileupCppTestSuite);

TEST_CASE(CSRA1_PileupIterator_GetDepth)
{
    char const db_path[] = "SRR341578";

    std::vector <uint32_t> vecDepthSlice, vecDepthEntire, vecRef;

    int64_t const pos_start = 20017;
    uint64_t const len = 5;

    vecRef.push_back(1); // 20017
    vecRef.push_back(0); // 20018
    vecRef.push_back(1); // 20019
    vecRef.push_back(1); // 20020
    vecRef.push_back(3); // 20021

    {
        ngs::ReadCollection run = ncbi::NGS::openReadCollection (db_path);
        ngs::ReferenceIterator ri = run.getReferences ();

        ri.nextReference ();
        ri.nextReference ();

        ngs::PileupIterator pi = ri.getPileups ( ngs::Alignment::primaryAlignment );

        uint64_t ref_pos = 0;
        for (; pi.nextPileup (); ++ ref_pos)
        {
            if ( ref_pos >= (uint64_t)pos_start && ref_pos < (uint64_t)pos_start + len )
                vecDepthEntire.push_back ( pi.getPileupDepth () );
        }
    }
    {
        ngs::ReadCollection run = ncbi::NGS::openReadCollection (db_path);
        ngs::ReferenceIterator ri = run.getReferences ();

        ri.nextReference ();
        ri.nextReference ();

        ngs::PileupIterator pi = ri.getPileupSlice ( pos_start, len, ngs::Alignment::primaryAlignment );

        uint64_t ref_pos = (uint64_t)pos_start;
        for (; pi.nextPileup (); ++ ref_pos)
        {
            if ( ref_pos >= (uint64_t)pos_start && ref_pos < (uint64_t)pos_start + len )
                vecDepthSlice.push_back ( pi.getPileupDepth () );
        }
    }

    REQUIRE_EQ ( vecRef.size(), vecDepthEntire.size() );
    REQUIRE_EQ ( vecRef.size(), vecDepthSlice.size() );

    for ( size_t i = 0; i < (size_t)len; ++i )
    {
        REQUIRE_EQ ( vecRef [i], vecDepthEntire [i] );
        REQUIRE_EQ ( vecRef [i], vecDepthSlice [i] );
    }
}


TEST_CASE(CSRA1_PileupEventIterator_GetType)
{
    char const db_path[] = "SRR341578";

    int64_t const pos_start = 20022;
    uint64_t const len = 1;

    ngs::ReadCollection run = ncbi::NGS::openReadCollection (db_path);
    ngs::ReferenceIterator ri = run.getReferences ();

    ri.nextReference ();
    ri.nextReference ();

    ngs::PileupEvent::PileupEventType arrRefEvents [] =
    {
        (ngs::PileupEvent::PileupEventType)(ngs::PileupEvent::mismatch | ngs::PileupEvent::alignment_minus_strand),
        ngs::PileupEvent::mismatch,
        ngs::PileupEvent::mismatch,
        (ngs::PileupEvent::PileupEventType)(ngs::PileupEvent::mismatch | ngs::PileupEvent::alignment_start),
        (ngs::PileupEvent::PileupEventType)(ngs::PileupEvent::mismatch | ngs::PileupEvent::alignment_minus_strand  | ngs::PileupEvent::alignment_start),
        (ngs::PileupEvent::PileupEventType)(ngs::PileupEvent::mismatch | ngs::PileupEvent::alignment_start)
    };

    ngs::PileupIterator pi = ri.getPileupSlice ( pos_start, len, ngs::Alignment::primaryAlignment );

    for (; pi.nextPileup (); )
    {
        REQUIRE_EQ ( pi.getPileupDepth(), (uint32_t)6 );
        for (size_t i = 0; pi.nextPileupEvent (); ++i)
        {
            REQUIRE_EQ ( pi.getEventType (), arrRefEvents [i] );
        }
    }
}

struct PileupEventStruct
{
    ngs::PileupEvent::PileupEventType event_type;
    uint32_t repeat_count, next_repeat_count;
    int mapping_quality;
    char alignment_base;
    bool deletion_after_this_pos;
    ngs::String insertion_bases;
};

struct PileupLine
{
    typedef std::vector <PileupEventStruct> TEvents;

    uint32_t depth;
    TEvents vecEvents;
};

void print_line (
    PileupLine const& line,
    char const* name,
    int64_t pos_start,
    int64_t pos,
    ngs::String const& strRefSlice,
    std::ostream& os)
{
    os
        << name
        << "\t" << (pos + 1)    // + 1 to be like sra-pileup - 1-based position
        << "\t" << strRefSlice [pos - pos_start]
        << "\t" << line.depth
        << "\t";

    for (PileupLine::TEvents::const_iterator cit = line.vecEvents.begin(); cit != line.vecEvents.end(); ++ cit)
    {
        PileupEventStruct const& pileup_event = *cit;

        ngs::PileupEvent::PileupEventType eventType = pileup_event.event_type;

        if ( ( eventType & ngs::PileupEvent::alignment_start ) != 0 )
        {
            char c = pileup_event.mapping_quality + 33;
            if ( c > '~' ) { c = '~'; }
            if ( c < 33 ) { c = 33; }

            os << "^" << c;
        }

        bool reverse = ( eventType & ngs::PileupEvent::alignment_minus_strand ) != 0;

        switch ( eventType & 7 )
        {
        case ngs::PileupEvent::match:
            os << (reverse ? "," : ".");
            break;
        case ngs::PileupEvent::mismatch:
            os
                << (reverse ?
                (char)tolower( pileup_event.alignment_base )
                : (char)toupper( pileup_event.alignment_base ));
            break;
        case ngs::PileupEvent::deletion:
            os << (reverse ? "<" : ">");
            break;
        }

        if ( pileup_event.insertion_bases.size() != 0 )
        {
            os
                << "+"
                << pileup_event.insertion_bases.size();

            for ( uint32_t i = 0; i < pileup_event.insertion_bases.size(); ++i )
            {
                os
                    << (reverse ?
                    (char)tolower(pileup_event.insertion_bases[i])
                    : (char)toupper(pileup_event.insertion_bases[i]));
            }
        }


        if ( pileup_event.deletion_after_this_pos )
        {
            uint32_t count = pileup_event.next_repeat_count;
            os << "-" << count;

            for ( uint32_t i = 0; i < count; ++i )
            {
                os
                    << (reverse ?
                    (char)tolower(strRefSlice [pos - pos_start + i + 1]) // + 1 means "deletion is at the NEXT position"
                    : (char)toupper(strRefSlice [pos - pos_start + i + 1])); // + 1 means "deletion is at the NEXT position"
            }

        }

        if ( ( eventType & ngs::PileupEvent::alignment_stop ) != 0 )
            os << "$";
    }
    os << std::endl;
}

void clear_line ( PileupLine& line )
{
    line.depth = 0;
    line.vecEvents.clear ();
}

void mark_line_as_starting_deletion ( PileupLine& line, uint32_t repeat_count, size_t alignment_index )
{
    PileupEventStruct& pileup_event = line.vecEvents [ alignment_index ];
    if ( ( pileup_event.event_type & 7 ) != ngs::PileupEvent::deletion)
    {
        pileup_event.next_repeat_count = repeat_count;
        pileup_event.deletion_after_this_pos = true;
    }
}

void mark_line_as_starting_insertion ( PileupLine& line, ngs::String const& insertion_bases, size_t alignment_index )
{
    PileupEventStruct& pileup_event = line.vecEvents [ alignment_index ];
    pileup_event.insertion_bases = insertion_bases;
}

void mimic_sra_pileup (
            char const* db_path,
            char const* ref_name,
            ngs::Alignment::AlignmentCategory category,
            int64_t const pos_start, uint64_t const len,
            std::ostream& os)
{
    ngs::ReadCollection run = ncbi::NGS::openReadCollection (db_path);
    ngs::Reference r = run.getReference ( ref_name );
    ngs::String const& canonical_name = r.getCanonicalName ();

    ngs::String strRefSlice = r.getReferenceBases ( pos_start, len );

    ngs::PileupIterator pi = r.getPileupSlice ( pos_start, len, category );

    PileupLine line_prev, line_curr;

    // maps current line alignment vector index to
    // previous line alignment vector index
    // mapAlignmentIdx[i] contains adjustment for index, not the absolute value
    std::vector <int64_t> mapAlignmentIdxPrev, mapAlignmentIdxCurr;

    int64_t pos = pos_start;
    for (; pi.nextPileup (); ++ pos)
    {
        line_curr.depth = pi.getPileupDepth ();
        line_curr.vecEvents.reserve (line_curr.depth);
        mapAlignmentIdxCurr.reserve (line_curr.depth);

        int64_t current_del_count = 0; // number of encountered stops

        for (; pi.nextPileupEvent (); )
        {
            PileupEventStruct pileup_event;

            //pileup_event.alignment_id = pi.getAlignmentId().toString();
            pileup_event.deletion_after_this_pos = false;
            pileup_event.event_type = pi.getEventType ();

            if ( ( pileup_event.event_type & ngs::PileupEvent::alignment_start ) != 0 )
                pileup_event.mapping_quality = pi.getMappingQuality();

            if ((pileup_event.event_type & 7) == ngs::PileupEvent::mismatch)
                pileup_event.alignment_base = pi.getAlignmentBase();

            if (pileup_event.event_type & ngs::PileupEvent::alignment_stop)
            {
                ++current_del_count;
                if (mapAlignmentIdxCurr.size() != 0)
                    mapAlignmentIdxCurr [ mapAlignmentIdxCurr.size() - 1 ] = current_del_count;
                else
                    mapAlignmentIdxCurr.push_back ( current_del_count );
            }
            else
            {
                mapAlignmentIdxCurr.push_back ( current_del_count );
            }

            if ( pos != pos_start )
            {
                // here in mapAlignmentIdxPrev we have already initialized
                // indicies for line_prev
                // so we can find corresponding alignment by doing:
                // int64_t idx = line_curr.vecEvents.size()
                // line_prev.vecEvents [ idx + mapAlignmentIdxPrev [idx] ]

                size_t idx_curr_align = line_curr.vecEvents.size();

                if (mapAlignmentIdxPrev.size() > idx_curr_align)
                {
                    if ((pileup_event.event_type & 7) == ngs::PileupEvent::deletion)
                        mark_line_as_starting_deletion ( line_prev, pi.getEventRepeatCount(), mapAlignmentIdxPrev [idx_curr_align] + idx_curr_align );
                    if ( pileup_event.event_type & ngs::PileupEvent::insertion )
                        mark_line_as_starting_insertion ( line_prev, pi.getInsertionBases().toString(), mapAlignmentIdxPrev [idx_curr_align] + idx_curr_align );
                }
            }

            line_curr.vecEvents.push_back ( pileup_event );
        }

        if ( pos != pos_start ) // there is no line_prev for the first line - nothing to print
        {
            // print previous line
            print_line ( line_prev, canonical_name.c_str(), pos_start, pos - 1, strRefSlice, os );
        }

        line_prev = line_curr;
        mapAlignmentIdxPrev = mapAlignmentIdxCurr;

        clear_line ( line_curr );
        mapAlignmentIdxCurr.clear();
    }
    // TODO: if the last line should contain insertion or deletion start ([-+]<number><seq>)
    // we have to look ahead 1 more position to be able to discover this and
    // modify line_prev, but if the last line is the very last one for the whole
    // reference - we shouldn't do that. This all isn't implemented yet in this function
    print_line ( line_prev, canonical_name.c_str(), pos_start, pos - 1, strRefSlice, os );
}

TEST_CASE(CSRA1_PileupEventIterator_Deletion)
{
    // deletions
    char const db_path[] = "SRR341578";

    int64_t const pos_start = 2427;
    int64_t const pos_end = 2428;
    uint64_t const len = (uint64_t)(pos_end - pos_start + 1);

    // The output must be the same as for "sra-pileup SRR341578 -r NC_011752.1:2428-2429 -s -n"
    std::ostringstream sstream;
    std::ostringstream sstream_ref;

    sstream_ref << "NC_011752.1\t2428\tG\t34\t..,.,.-1A.-1A.-1A.-1A.-1A.-1A,-1a,-1a.-1A,-1a,-1a.-1A.-1A.-1A,-1a,-1a,-1a,-1a.-1A,-1a,-1a.-1A,-1a.-1A.-1A,-1a.^F,^F," << std::endl;
    sstream_ref << "NC_011752.1\t2429\tA\t34\t.$.$,$.,>>>>>><<><<>>><<<<><<><>><Ggg" << std::endl;

    mimic_sra_pileup ( db_path, "gi|218511148|ref|NC_011752.1|", ngs::Alignment::all, pos_start, len, sstream );

    REQUIRE_EQ ( sstream.str (), sstream_ref.str () );
}

TEST_CASE(CSRA1_PileupEventIterator_Insertion)
{
    // simple matches, mismatch, insertion, mapping quality
    char const db_path[] = "SRR341578";

    int64_t const pos_start = 2017;
    int64_t const pos_end = 2018;
    uint64_t const len = (uint64_t)(pos_end - pos_start + 1);

    // The output must be the same as for "sra-pileup SRR341578 -r NC_011752.1:2018-2019 -s -n"
    std::ostringstream sstream;
    std::ostringstream sstream_ref;

    sstream_ref << "NC_011752.1\t2018\tT\t17\t.....,,A,,..+2CA..^F.^F.^:N" << std::endl;
    sstream_ref << "NC_011752.1\t2019\tC\t19\t.....,,.,,.......^F.^F," << std::endl;

    mimic_sra_pileup ( db_path, "gi|218511148|ref|NC_011752.1|", ngs::Alignment::all, pos_start, len, sstream );

    REQUIRE_EQ ( sstream.str (), sstream_ref.str () );
}

TEST_CASE(CSRA1_PileupEventIterator_TrickyInsertion)
{
    // the insertion occurs in 1 or more previous chunks but not the current

    char const db_path[] = "SRR341578";

    int64_t const pos_start = 380000;
    int64_t const pos_end = 380001;
    uint64_t const len = (uint64_t)(pos_end - pos_start + 1);

    // The output must be the same as for "sra-pileup SRR341578 -r NC_011748.1:380001-380002 -s -n"
    std::ostringstream sstream;
    std::ostringstream sstream_ref;

    sstream_ref << "NC_011748.1\t380001\tT\t61\t....,,...,......,,...,,.....,,..,.,,,,...,,,,,,,+2tc.,.....G....," << std::endl;
    sstream_ref << "NC_011748.1\t380002\tT\t61\t.$.$.$.$,$,$...,......,,...,,.....,,A.,.,,,,...,,,,,,,.,.....G....," << std::endl;

    mimic_sra_pileup ( db_path, "gi|218693476|ref|NC_011748.1|", ngs::Alignment::primaryAlignment, pos_start, len, sstream );

    REQUIRE_EQ ( sstream.str (), sstream_ref.str () );
}


TEST_CASE(CSRA1_PileupIterator_StartingZeros)
{
    // this is transition from depth == 0 to depth == 1
    // initial code had different output for primaryAlignments vs all

    int64_t const pos_start = 19374;
    int64_t const pos_end = 19375;
    uint64_t const len = (uint64_t)(pos_end - pos_start + 1); //3906625;

    // when requesting category == all, the output must be the same as with
    // primaryAlignments
    // reference output: sra-pileup SRR833251 -r "gi|169794206|ref|NC_010410.1|":19375-19376 -s -n

    std::ostringstream sstream;
    std::ostringstream sstream_ref;

    sstream_ref << "gi|169794206|ref|NC_010410.1|\t19375\tC\t0\t" << std::endl;
    sstream_ref << "gi|169794206|ref|NC_010410.1|\t19376\tA\t1\t^!." << std::endl;

    mimic_sra_pileup ( "SRR833251", "gi|169794206|ref|NC_010410.1|", ngs::Alignment::all, pos_start, len, sstream );

    REQUIRE_EQ ( sstream.str (), sstream_ref.str () );
}



// Kurt's test to investigate:

// TODO: make test out of the following run() function
namespace ref_cover
{
    using namespace ngs;
    static
    void run ( const String & runName, const String & refName, PileupIterator & pileup )
    {
        for ( int64_t ref_zpos = -1; pileup . nextPileup (); ++ ref_zpos )
        {
            if ( ref_zpos < 0 )
                ref_zpos = pileup . getReferencePosition ();

            uint32_t ref_base_idx = 0;
            char ref_base = pileup . getReferenceBase ();
            switch ( ref_base )
            {
            case 'C': ref_base_idx = 1; break;
            case 'G': ref_base_idx = 2; break;
            case 'T': ref_base_idx = 3; break;
            }

            uint32_t depth = pileup . getPileupDepth ();
            if ( depth != 0 )
            {
                uint32_t base_counts [ 4 ];
                memset ( base_counts, 0, sizeof base_counts );
                uint32_t ins_counts [ 4 ];
                memset ( ins_counts, 0, sizeof ins_counts );
                uint32_t del_cnt = 0;

                char mismatch;
                uint32_t mismatch_idx;

                while ( pileup . nextPileupEvent () )
                {
                    PileupEvent :: PileupEventType et = pileup . getEventType ();
                    switch ( et & 7 )
                    {
                    case PileupEvent :: match:
                        if ( ( et & PileupEvent :: insertion ) != 0 )
                            ++ ins_counts [ ref_base_idx ];
                        break;

                    case PileupEvent :: mismatch:
                        mismatch = pileup . getAlignmentBase ();
                        mismatch_idx = 0;
                        switch ( mismatch )
                        {
                        case 'C': mismatch_idx = 1; break;
                        case 'G': mismatch_idx = 2; break;
                        case 'T': mismatch_idx = 3; break;
                        }
                        ++ base_counts [ mismatch_idx ];
                        if ( ( et & PileupEvent :: insertion ) != 0 )
                            ++ ins_counts [ mismatch_idx ];
                        break;

                    case PileupEvent :: deletion:
                        if ( pileup . getEventIndelType () == PileupEvent :: normal_indel )
                            ++ del_cnt;
                        else
                            -- depth;
                        break;
                    }
                }

                if ( depth != 0 )
                {
                    std :: cout
                        << runName
                        << '\t' << refName
                        << '\t' << ref_zpos + 1
                        << '\t' << ref_base
                        << '\t' << depth
                        << "\t{" << base_counts [ 0 ]
                        << ',' << base_counts [ 1 ]
                        << ',' << base_counts [ 2 ]
                        << ',' << base_counts [ 3 ]
                        << "}\t{" << ins_counts [ 0 ]
                        << ',' << ins_counts [ 1 ]
                        << ',' << ins_counts [ 2 ]
                        << ',' << ins_counts [ 3 ]
                        << "}\t" << del_cnt
                        << '\n'
                        ;
                }
            }
        }
    }

    static
    void run ( const char * spec )
    {
        std :: cerr << "# Opening run '" << spec << "'\n";
        ReadCollection obj = ncbi :: NGS :: openReadCollection ( spec );
        String runName = obj . getName ();

        std :: cerr << "# Accessing all references\n";
        ReferenceIterator ref = obj . getReferences ();

        while ( ref . nextReference () )
        {
            String refName = ref . getCanonicalName ();
            //String const& ref_full = ref.getReferenceBases(10000, 1000);

            std :: cerr << "# Processing reference '" << refName << "'\n";

            std :: cerr << "# Accessing all pileups\n";
            PileupIterator pileup = ref . getPileups ( Alignment :: all );
            run ( runName, refName, pileup );
        }
    }
}


int fake_main (int argc, char const* const* argv)
{
    rc_t rc = 0;

    try
    {
        for ( int i = 1; i < argc; ++ i )
        {
            ref_cover :: run ( argv [ i ] );
        }
    }
    catch ( ngs::ErrorMsg & x )
    {
        std :: cerr
            << "ERROR: "
            << argv [ 0 ]
        << ": "
            << x . what ()
            << '\n'
            ;
        rc = -1;
    }
    catch ( ... )
    {
        std :: cerr
            << "ERROR: "
            << argv [ 0 ]
        << ": unknown\n"
            ;
        rc = -1;
    }

    return rc;
}


uint64_t pileup_test_all_functions (
            char const* db_path,
            char const* ref_name,
            ngs::Alignment::AlignmentCategory category,
            int64_t const pos_start, uint64_t const len)
{
    uint64_t ret = 0;

    ngs::ReadCollection run = ncbi::NGS::openReadCollection (db_path);
    ngs::Reference r = run.getReference ( ref_name );
    ngs::String strRefSlice = r.getReferenceBases ( pos_start, len );

    ngs::PileupIterator pi = r.getPileupSlice ( pos_start, len, category );

    int64_t pos = pos_start;
    for (; pi.nextPileup (); ++ pos)
    {
        ret += 1000000;

        size_t event_count = 0;
        for (; pi.nextPileupEvent () && pos % 17 != 0; ++ event_count)
        {
            //ngs::Alignment alignment = pi.getAlignment();
            //ret += (uint64_t)(alignment.getAlignmentLength() + alignment.getAlignmentPosition());

            ret += (uint64_t)pi.getAlignmentBase();
            ret += (uint64_t)pi.getAlignmentPosition();
            ret += (uint64_t)pi.getAlignmentQuality();
            ret += (uint64_t)pi.getEventIndelType();
            ret += (uint64_t)pi.getEventRepeatCount();
            ret += (uint64_t)pi.getEventType();
            ret += (uint64_t)pi.getFirstAlignmentPosition();
            ret += (uint64_t)pi.getInsertionBases().size();
            ret += (uint64_t)pi.getInsertionQualities().size();
            ret += (uint64_t)pi.getLastAlignmentPosition();
            ret += (uint64_t)pi.getMappingQuality();
            ret += (uint64_t)pi.getPileupDepth();
            ret += (uint64_t)pi.getReferenceBase();
            ret += (uint64_t)pi.getReferencePosition();
            ret += (uint64_t)pi.getReferenceSpec().size();

            if ( (event_count + 1) % 67 == 0 )
            {
                ret += 100000;
                pi.resetPileupEvent();
                break;
            }
        }
    }

    return ret;
}

TEST_CASE(CSRA1_PileupIterator_TestAllFunctions)
{
    uint64_t ret = 0;
    ret = pileup_test_all_functions ( "SRR822962", "chr2"/*"NC_000002.11"*/, ngs::Alignment::all, 0, 20000 );
    REQUIRE_EQ ( ret, (uint64_t)46433887435 );
}

/*
TEST_CASE(CSRA1_PileupEvent_Coverage)
{
    char const* argv[] = {
        "fake_line",
        "SRR822962"
    };

    int res = fake_main ( sizeof(argv)/sizeof (argv[0]) , argv );

    REQUIRE_EQ ( res, 0 );
}
*/


//////////////////////////////////////////// Main
extern "C"
{

#include <kapp/args.h>

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}
rc_t CC UsageSummary (const char * progname)
{
    return 0;
}

rc_t CC Usage ( const Args * args )
{
    return 0;
}

const char UsageDefaultName[] = "test-ngs_csra1pileup-c++";

rc_t CC KMain ( int argc, char *argv [] )
{
    rc_t rc=NgsCsra1PileupCppTestSuite(argc, argv);
    return rc;
}

}


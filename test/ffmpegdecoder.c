// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <munit.h>

#include <chiaki/ffmpegdecoder.h>

#include <libavutil/frame.h>
#include <libavutil/avutil.h>

/* Helper: allocate an AVFrame with both timestamp fields set to AV_NOPTS_VALUE */
static AVFrame *alloc_blank_frame(void)
{
	AVFrame *f = av_frame_alloc();
	munit_assert_not_null(f);
	f->best_effort_timestamp = AV_NOPTS_VALUE;
	f->pts                   = AV_NOPTS_VALUE;
	return f;
}

/* pts = best_effort_timestamp * av_q2d(pkt_timebase) */
static MunitResult test_pts_from_best_effort(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->best_effort_timestamp = 12345;

	double pts = 0.0, dur = 0.0;
	/* 1/90000 timebase, 60 fps */
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){1, 90000},  /* pkt_timebase */
		(AVRational){0, 0},      /* ctx_timebase (invalid → not used) */
		(AVRational){60, 1},     /* framerate */
		&pts, &dur);

	munit_assert_double_equal(pts, 12345.0 / 90000.0, 9);
	munit_assert_double_equal(dur, 1.0 / 60.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* best_effort_timestamp == AV_NOPTS_VALUE → fall back to frame->pts */
static MunitResult test_pts_fallback_to_pts(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->pts = 9000;  /* best_effort stays NOPTS */

	double pts = 0.0, dur = 0.0;
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){1, 90000},
		(AVRational){0, 0},
		(AVRational){30, 1},
		&pts, &dur);

	munit_assert_double_equal(pts, 9000.0 / 90000.0, 9);
	munit_assert_double_equal(dur, 1.0 / 30.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* both timestamp fields == AV_NOPTS_VALUE → pts == 0.0 */
static MunitResult test_pts_both_nopts(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	/* both stay AV_NOPTS_VALUE */

	double pts = -1.0, dur = 0.0;
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){1, 90000},
		(AVRational){0, 0},
		(AVRational){60, 1},
		&pts, &dur);

	munit_assert_double_equal(pts, 0.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* pkt_timebase invalid → fall through to ctx_timebase */
static MunitResult test_timebase_fallback_to_ctx(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->best_effort_timestamp = 1000;

	double pts = 0.0, dur = 0.0;
	/* pkt_timebase invalid, ctx_timebase = 1/1000 */
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){0, 0},    /* invalid pkt_timebase */
		(AVRational){1, 1000}, /* ctx_timebase */
		(AVRational){60, 1},
		&pts, &dur);

	munit_assert_double_equal(pts, 1000.0 / 1000.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* both timebases invalid → fall back to 1/1000000 (microsecond timebase) */
static MunitResult test_timebase_fallback_to_default(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->best_effort_timestamp = 1000000; /* 1 second in µs */

	double pts = 0.0, dur = 0.0;
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){0, 0},  /* invalid */
		(AVRational){0, 0},  /* invalid */
		(AVRational){60, 1},
		&pts, &dur);

	munit_assert_double_equal(pts, 1.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* framerate invalid (0/0) → fps falls back to 60, duration = 1/60 */
static MunitResult test_duration_framerate_fallback(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->best_effort_timestamp = 0;

	double pts = 0.0, dur = 0.0;
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){1, 90000},
		(AVRational){0, 0},
		(AVRational){0, 0},  /* invalid framerate */
		&pts, &dur);

	munit_assert_double_equal(dur, 1.0 / 60.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

/* 120 fps → duration = 1/120 */
static MunitResult test_duration_120fps(const MunitParameter params[], void *user)
{
	AVFrame *frame = alloc_blank_frame();
	frame->best_effort_timestamp = 0;

	double pts = 0.0, dur = 0.0;
	chiaki_ffmpeg_frame_get_timing(
		frame,
		(AVRational){1, 90000},
		(AVRational){0, 0},
		(AVRational){120, 1},
		&pts, &dur);

	munit_assert_double_equal(dur, 1.0 / 120.0, 9);

	av_frame_free(&frame);
	return MUNIT_OK;
}

static ChiakiFfmpegDecoder timing_decoder(unsigned int fps)
{
	ChiakiFfmpegDecoder decoder = {0};
	decoder.synthetic_last_packet_pts = -1;
	decoder.synthetic_framerate = (AVRational){(int)fps, 1};
	decoder.synthetic_frame_duration_us = 1000000.0 / fps;
	decoder.synthetic_candidate_duration_us = decoder.synthetic_frame_duration_us;
	return decoder;
}

/* Exercise the production clock for an hour without sleeping or a console.
 * Both signs of clock skew and sub-microsecond duration rounding matter. */
static MunitResult test_long_session_drift(const MunitParameter params[], void *user)
{
	const double rates[] = {60.0, 60000.0 / 1001.0, 60.06, 30.0, 30000.0 / 1001.0};
	for(size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); r++)
	{
		ChiakiFfmpegDecoder decoder = timing_decoder(r < 3 ? 60 : 30);
		int64_t previous = -1;
		int64_t tolerance = r < 3 ? 33333 : 66667;
		for(int i = 0; i < (int)(rates[r] * 3600); i++)
		{
			uint64_t elapsed = (uint64_t)(i * 1000000.0 / rates[r] + 0.5);
			int64_t pts = chiaki_ffmpeg_decoder_next_pts(&decoder, 1000000 + elapsed, 0);
			munit_assert_int64(pts, >, previous);
			munit_assert_int64(pts - (int64_t)elapsed, <=, tolerance);
			munit_assert_int64((int64_t)elapsed - pts, <=, tolerance);
			previous = pts;
		}
	}
	return MUNIT_OK;
}

static MunitResult test_timing_jitter_and_loss(const MunitParameter params[], void *user)
{
	ChiakiFfmpegDecoder decoder = timing_decoder(60);
	munit_assert_int64(chiaki_ffmpeg_decoder_next_pts(&decoder, 0, 0), ==, 0);
	/* Ordinary arrival jitter must not become presentation jitter. */
	for(int i = 1; i < 1000; i++)
	{
		uint64_t arrival = (uint64_t)(i * 1000000.0 / 60.0) + (i % 2 ? 2000 : 0);
		munit_assert_int64(chiaki_ffmpeg_decoder_next_pts(&decoder, arrival, 0), ==, i * 16667);
	}
	/* Lost frames still leave a gap on the nominal presentation timeline. */
	int64_t previous = decoder.synthetic_last_packet_pts;
	int64_t pts = chiaki_ffmpeg_decoder_next_pts(&decoder, 1002000000ULL / 60, 2);
	munit_assert_int64(pts - previous, ==, 3 * 16667);
	/* A five-second interruption must not leave playback five seconds behind. */
	uint64_t resumed = decoder.synthetic_last_sample_time_us + 5000000;
	pts = chiaki_ffmpeg_decoder_next_pts(&decoder, resumed, 0);
	munit_assert_int64((int64_t)resumed - pts, <=, 33333);
	/* Draining a burst must never roll timestamps back or duplicate them. */
	for(int i = 0; i < 120; i++)
	{
		previous = pts;
		pts = chiaki_ffmpeg_decoder_next_pts(&decoder, resumed, 0);
		munit_assert_int64(pts, >, previous + 1);
		munit_assert_int64(pts - (int64_t)resumed, <, 34000);
	}
	return MUNIT_OK;
}

static MunitResult test_timing_rate_switch(const MunitParameter params[], void *user)
{
	ChiakiFfmpegDecoder decoder = timing_decoder(60);
	uint64_t now = 1000000;
	int64_t previous = chiaki_ffmpeg_decoder_next_pts(&decoder, now, 0);
	for(int segment = 0; segment < 3; segment++)
	{
		uint64_t interval = segment == 1 ? 33333 : 16667;
		for(int i = 0; i < 600; i++)
		{
			now += interval;
			int64_t pts = chiaki_ffmpeg_decoder_next_pts(&decoder, now, 0);
			munit_assert_int64(pts, >, previous);
			munit_assert_int64(pts - (int64_t)(now - 1000000), <=, 33333);
			munit_assert_int64((int64_t)(now - 1000000) - pts, <=, 33333);
			previous = pts;
		}
		munit_assert_double_equal(decoder.synthetic_frame_duration_us, (double)interval, 0);
	}
	return MUNIT_OK;
}

MunitTest tests_ffmpegdecoder[] = {
	{ "/long_session_drift", test_long_session_drift, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/timing_jitter_and_loss", test_timing_jitter_and_loss, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{ "/timing_rate_switch", test_timing_rate_switch, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL },
	{
		"/pts_from_best_effort",
		test_pts_from_best_effort,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/pts_fallback_to_pts",
		test_pts_fallback_to_pts,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/pts_both_nopts",
		test_pts_both_nopts,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/timebase_fallback_to_ctx",
		test_timebase_fallback_to_ctx,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/timebase_fallback_to_default",
		test_timebase_fallback_to_default,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/duration_framerate_fallback",
		test_duration_framerate_fallback,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{
		"/duration_120fps",
		test_duration_120fps,
		NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL
	},
	{ NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

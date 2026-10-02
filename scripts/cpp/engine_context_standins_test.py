#!/usr/bin/env python3
# GeneralsX @feature cemlyn007 02/10/2026 Tests for engine_context_standins.py's uses() (review fix for PLAN-023
# Phase 8, stage RR2a-1: the sizeof/& gate missed a cast before unary `&`, `&&` before unary `&`,
# `std::addressof`, and a stand-in passed bare through a variadic logger).
#
# Usage: python3 engine_context_standins_test.py

import sys
import unittest

import engine_context_standins as ecs


class UsesTest(unittest.TestCase):
    def assert_flagged(self, text, name="BitDepth", qualifier=""):
        bad = ecs.uses(text, name, qualifier)
        self.assertTrue(bad, f"expected {text!r} to be flagged, got {bad!r}")

    def assert_clean(self, text, name="BitDepth", qualifier=""):
        bad = ecs.uses(text, name, qualifier)
        self.assertFalse(bad, f"expected {text!r} not to be flagged, got {bad!r}")

    # Unary `&`, the fixed set of operators the original gate already covered.
    def test_plain_unary_ampersand(self):
        self.assert_flagged("&BitDepth")
        self.assert_flagged("x = &BitDepth")

    def test_binary_ampersand_is_not_flagged(self):
        self.assert_clean("x & BitDepth")
        self.assert_clean("BitDepth & mask")

    def test_reaching_the_field_is_not_flagged(self):
        self.assert_clean("&BitDepth.x")
        self.assert_clean("&BitDepth[0]")
        self.assert_clean("&BitDepth->x")

    # A C-style pointer cast before unary `&`: the gap this review found.
    def test_cast_before_ampersand(self):
        self.assert_flagged("(void*)&BitDepth")
        self.assert_flagged("(char*)&Textures", name="Textures")
        self.assert_flagged("(IDirect3DBaseTexture8**)&Textures", name="Textures")

    def test_cast_without_pointer_is_not_flagged(self):
        # Indistinguishable from `(expr)&mask`, a binary `&` of a parenthesised expression.
        self.assert_clean("(int)&BitDepth")

    # `&&` before unary `&`: also missed before this review.
    def test_double_ampersand_then_unary_ampersand(self):
        self.assert_flagged("a && &BitDepth")
        self.assert_flagged("a&& &BitDepth")

    def test_double_ampersand_token_is_not_flagged(self):
        # `a &&BitDepth` is always logical-and of `a` and `BitDepth`, never unary `&` of `&BitDepth`.
        self.assert_clean("a &&BitDepth")
        self.assert_clean("a && BitDepth")

    # std::addressof: the same hazard as unary `&`, not covered at all before this review.
    def test_std_addressof(self):
        self.assert_flagged("std::addressof(BitDepth)")
        self.assert_flagged("std::addressof( BitDepth )")

    def test_std_addressof_reaching_the_field_is_not_flagged(self):
        self.assert_clean("std::addressof(BitDepth.x)")

    # A stand-in passed bare through a variadic logger: not covered at all before this review.
    def test_variadic_logger(self):
        self.assert_flagged('WWDEBUG_SAY(("bits %d", BitDepth))')
        self.assert_flagged('DEBUG_LOG(("bits %d", BitDepth))')
        self.assert_flagged('fprintf(stderr, "%d", BitDepth)')

    def test_variadic_logger_reaching_the_field_is_not_flagged(self):
        self.assert_clean('WWDEBUG_SAY(("bits %d", BitDepth.x))')

    def test_unrelated_call_is_not_flagged(self):
        self.assert_clean("some_other_function(BitDepth)")

    def test_qualified_name(self):
        self.assert_flagged("(void*)&DX8Wrapper::BitDepth", qualifier="DX8Wrapper")
        self.assert_clean("DX8Wrapper::BitDepth & mask", qualifier="DX8Wrapper")


if __name__ == "__main__":
    sys.exit(unittest.main())

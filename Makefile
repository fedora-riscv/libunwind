# Makefile for source rpm: libunwind
# $Id$
NAME := libunwind
SPECFILE = $(firstword $(wildcard *.spec))

include ../common/Makefile.common

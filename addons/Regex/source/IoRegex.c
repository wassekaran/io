/*#io
Regex ioDoc(
	docCopyright("Steve Dekorte", 2005)
	docCopyright("Daniel Rosengren", 2007)
	docLicense("BSD revised")
	docCategory("RegularExpressions")

	docDescription("""The Regex addon adds support for Perl regular expressions 
	using the <a href=http://www.pcre.org/>PCRE</a> library by Philip Hazel.

	Example use:
	<pre>
	Io> "11aabb" allMatchesOfRegex("aa*")
	==> list("a", "a")
	
	Io> re := "(wom)(bat)" asRegex
	Io> "wombats are cuddly" matchesOfRegex(re) replaceWith("$2$1!")
	==> batwom!s are cuddly
	</pre>

	<blockquote>
	Some people, when confronted with a problem, think
	"I know, I'll use regular expressions."
	Now they have two problems.
	</blockquote>
	<strong>Jamie Zawinski</strong>
	""")
*/

#include "IoRegex.h"
#include "IoState.h"
#include "IoNumber.h"
#include "IoList.h"
#include <stdlib.h>
#include <stdio.h>

#define DATA(self) ((IoRegexData *)IoObject_dataPointer(self))

static IoRegex *IoRegex_cloneWithOptions_(IoRegex *self, int options);


IoTag *IoRegex_newTag(void *state)
{
	IoTag *tag = IoTag_newWithName_("Regex");
	IoTag_state_(tag, state);
	IoTag_freeFunc_(tag, (IoTagFreeFunc *)IoRegex_free);
	IoTag_cloneFunc_(tag, (IoTagCloneFunc *)IoRegex_rawClone);
	IoTag_markFunc_(tag, (IoTagMarkFunc *)IoRegex_mark);
	return tag;
}

IoRegex *IoRegex_proto(void *state)
{
	IoObject *self = IoObject_new(state);
	IoObject_tag_(self, IoRegex_newTag(state));
	
	IoObject_setDataPointer_(self, calloc(1, sizeof(IoRegexData)));
	DATA(self)->pattern = IOSYMBOL("");

	IoState_registerProtoWithFunc_(state, self, IoRegex_proto);

	{
		IoMethodTable methodTable[] = {
			{"with", IoRegex_with},

			{"pattern", IoRegex_pattern},
			{"captureCount", IoRegex_captureCount},
			{"nameToIndexMap", IoRegex_nameToIndexMap},

			{"version", IoRegex_version},

			/* Options */
			
			{"caseless", IoRegex_caseless},
			{"notCaseless", IoRegex_notCaseless},
			{"isCaseless", IoRegex_isCaseless},

			{"dotAll", IoRegex_dotAll},
			{"notDotAll", IoRegex_notDotAll},
			{"isDotAll", IoRegex_isDotAll},

			{"extended", IoRegex_extended},
			{"notExtended", IoRegex_notExtended},
			{"isExtended", IoRegex_isExtended},

			{"multiline", IoRegex_multiline},
			{"notMultiline", IoRegex_notMultiline},
			{"isMultiline", IoRegex_isMultiline},

			{0, 0},
		};
		
		IoObject_addMethodTable_(self, methodTable);
	}

	return self;
}

IoRegex *IoRegex_rawClone(IoRegex *proto)
{
	IoObject *self = IoObject_rawClonePrimitive(proto);
	IoObject_setDataPointer_(self, calloc(1, sizeof(IoRegexData)));
	DATA(self)->pattern = IOREF(DATA(proto)->pattern);
	return self;
}

IoRegex *IoRegex_new(void *state)
{
	return IOCLONE(IoState_protoWithInitFunction_(state, IoRegex_proto));
}

IoRegex *IoRegex_newWithPattern_(void *state, IoSymbol *pattern)
{
	IoRegex *self = IoRegex_new(state);
	DATA(self)->pattern = IOREF(pattern);
	return self;
}

void IoRegex_free(IoRegex *self)
{
	if (DATA(self)->regex)
		Regex_free(DATA(self)->regex);
	free(DATA(self));
}

void IoRegex_mark(IoRegex *self)
{
	IoObject_shouldMark(DATA(self)->pattern);
	if (DATA(self)->nameToIndexMap)
		IoObject_shouldMark(DATA(self)->nameToIndexMap);
}


Regex *IoRegex_rawRegex(IoRegex *self)
{
	if (DATA(self)->regex)
		return DATA(self)->regex;
	
	DATA(self)->regex = Regex_newFromPattern_withOptions_(
		CSTRING(DATA(self)->pattern),
		DATA(self)->options
	);
	return DATA(self)->regex;
}


/* ------------------------------------------------------------------------------------------------*/

IoObject *IoRegex_with(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("with(pattern)",
		"Returns a new Regex created from the given pattern string.")
	*/
	return IoRegex_newWithPattern_(IOSTATE, IoMessage_locals_symbolArgAt_(m, locals, 0));
}


IoObject *IoRegex_pattern(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("pattern",
		"Returns the pattern string that the receiver was created from.")
	*/
	return DATA(self)->pattern;
}

IoObject *IoRegex_captureCount(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("captureCount",
		"Returns the number of captures defined by the pattern.")
	*/
	return IONUMBER(IoRegex_rawRegex(self)->captureCount);
}

IoObject *IoRegex_nameToIndexMap(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("nameToIndexMap",
		"Returns a Map that maps capture names to capture indices.")
	*/
	IoMap *map = DATA(self)->nameToIndexMap;
	Regex *regex = 0;
	NamedCapture *namedCaptures = 0, *capture = 0;
	
	if (map)
		return map;

	map = DATA(self)->nameToIndexMap = IOREF(IoMap_new(IOSTATE));

	capture = namedCaptures = Regex_namedCaptures(IoRegex_rawRegex(self));
	if (!namedCaptures)
		return map;
	
	while (capture->name) {
		IoMap_rawAtPut(map, IOSYMBOL(capture->name), IONUMBER(capture->index));
		capture++;
	}
	free(namedCaptures);
	return map;
}


IoObject *IoRegex_version(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("version",
		"Returns a string with PCRE version information.")
	*/
	return IOSYMBOL(pcre_version());
}


/* ------------------------------------------------------------------------------------------------*/
/* Options */

IoObject *IoRegex_caseless(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("caseless",
		"""Returns a case insensitive clone of the receiver, or self if the receiver itself is
		case insensitive.
		
		Example:
		<pre>
		Io> "WORD" matchesRegex("[a-z]+")
		==> false
		
		Io> "WORD" matchesRegex("[a-z]+" asRegex caseless)
		==> true
		</pre>""")
	*/
	return IoRegex_cloneWithOptions_(self, DATA(self)->options | PCRE_CASELESS);
}

IoObject *IoRegex_notCaseless(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("notCaseless",
		"The reverse of caseless.")
	*/
	return IoRegex_cloneWithOptions_(self, DATA(self)->options & ~PCRE_CASELESS);
}

IoObject *IoRegex_isCaseless(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("isCaseless",
		"Returns true if the receiver is case insensitive, false if not.")
	*/
	return IOBOOL(self, DATA(self)->options & PCRE_CASELESS);
}


IoObject *IoRegex_dotAll(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("dotAll",
		"""Returns a clone of the receiver with the dotall option turned on,
		or self if the receiver itself has the option turned on.

		In dotall mode, "." matches any character, including newline. By default
		it matches any character <em>except</em> newline.
		
		Example:
		<pre>
		Io> "A\nB" matchesOfRegex(".+") next string
		==> A
		
		Io> "A\nB" matchesOfRegex(".+" asRegex dotAll) next string
		==> A\nB
		</pre>""")
	*/
	return IoRegex_cloneWithOptions_(self, DATA(self)->options | PCRE_DOTALL);
}

IoObject *IoRegex_notDotAll(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("notDotAll",
		"The reverse of dotAll.")
	*/
	return IoRegex_cloneWithOptions_(self, DATA(self)->options & ~PCRE_DOTALL);
}

IoObject *IoRegex_isDotAll(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("isDotAll",
		"Returns true if the receiver is in dotall mode, false if not.")
	*/
	return IOBOOL(self, DATA(self)->options & PCRE_DOTALL);
}


IoObject *IoRegex_extended(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("extended",
		"""Returns a clone of the receiver with the extended option turned on,
		or self if the receiver itself has the option turned on.
		
		In extended mode, a Regex ignores any whitespace character in the pattern	except
		when escaped or inside a character class. This allows you to write clearer patterns
		that may be broken up into several lines.
		
		Additionally, you can put comments in the pattern. A comment starts with a "#"
		character and continues to the end of the line, unless the "#" is escaped or is
		inside a character class.""")
	*/
	return IoRegex_cloneWithOptions_(self, DATA(self)->options | PCRE_EXTENDED);
}

IoObject *IoRegex_notExtended(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("notExtended",
		"The reverse of extended.")
	*/
	return IoRegex_cloneWithOptions_(self, DATA(self)->options & ~PCRE_EXTENDED);
}

IoObject *IoRegex_isExtended(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("isExtended",
		"Returns true if the receiver is in extended mode, false if not.")
	*/
	return IOBOOL(self, DATA(self)->options & PCRE_EXTENDED);
}


IoObject *IoRegex_multiline(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("multiline",
		"""Returns a clone of the receiver with the multiline option turned on,
		or self if the receiver itself has the option turned on.
		
		In multiline mode, "^" matches at the beginning of the string and at
		the beginning of each line; and "$" matches at the end of the string,
		and at the end of each line.
		By default "^" only matches at the beginning of the string, and "$"
		only matches at the end of the string.
		
		Example:
		<pre>
		Io> "A\nB\nC" allMatchesForRegex("^.")
		==> list("A")
		
		Io> "A\nB\nC" allMatchesForRegex("^." asRegex multiline)
		==> list("A", "B", "C")
		</pre>
		""")
	*/
	return IoRegex_cloneWithOptions_(self, DATA(self)->options | PCRE_MULTILINE);
}

IoObject *IoRegex_notMultiline(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("notMultiline",
		"The reverse of multiline.")
	*/
	return IoRegex_cloneWithOptions_(self, DATA(self)->options & ~PCRE_MULTILINE);
}

IoObject *IoRegex_isMultiline(IoRegex *self, IoObject *locals, IoMessage *m)
{
	/*#io
	docSlot("isMultiline",
		"Returns true if the receiver is in multiline mode, false if not.")
	*/
	return IOBOOL(self, DATA(self)->options & PCRE_MULTILINE);
}


/* ------------------------------------------------------------------------------------------------*/
/* Private */

static IoRegex *IoRegex_cloneWithOptions_(IoRegex *self, int options)
{
	IoRegex *clone = 0;

	if (options == DATA(self)->options)
		return self;

	clone = IOCLONE(self);
	DATA(clone)->options = options;
	return clone;
}

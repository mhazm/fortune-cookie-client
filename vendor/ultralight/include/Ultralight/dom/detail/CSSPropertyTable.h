///
/// Copyright (C) 2026 Ultralight, Inc. All rights reserved.
/// A license is required for commercial use. https://ultralig.ht
///
/// GENERATED FILE, DO NOT HAND-EDIT. Produced by scripts/generate_css_properties.py
/// from deps/WebCore/Source/WebCore/css/CSSProperties.json; regenerate with:
///
///     python scripts/generate_css_properties.py
///
/// Implementation detail of the `<Ultralight/dom/*.h>` headers; include those instead.
/// A build-independent CSS property name table: (identifier, css-name) rows for every
/// web-facing property, plus an alias table (legacy spellings mapping to their
/// canonical row), plus every row of both sorted by identifier.
///
/// The table is append-only: every row is a name customer code compiles against, so a
/// row that shipped stays even if the engine drops the property (see the generator).
///
/// \cond INTERNAL
#pragma once

#define ULDOM_CSS_PROPERTY_COUNT 518
#define ULDOM_CSS_PROPERTY_ALIAS_COUNT 130

// V(identifier, cssName)
#define ULDOM_FOR_EACH_CSS_PROPERTY(V) \
  V(webkitBackgroundClip, "-webkit-background-clip") \
  V(webkitBackgroundOrigin, "-webkit-background-origin") \
  V(webkitBackgroundSize, "-webkit-background-size") \
  V(webkitBorderHorizontalSpacing, "-webkit-border-horizontal-spacing") \
  V(webkitBorderImage, "-webkit-border-image") \
  V(webkitBorderRadius, "-webkit-border-radius") \
  V(webkitBorderVerticalSpacing, "-webkit-border-vertical-spacing") \
  V(webkitBoxAlign, "-webkit-box-align") \
  V(webkitBoxDecorationBreak, "-webkit-box-decoration-break") \
  V(webkitBoxDirection, "-webkit-box-direction") \
  V(webkitBoxFlex, "-webkit-box-flex") \
  V(webkitBoxFlexGroup, "-webkit-box-flex-group") \
  V(webkitBoxLines, "-webkit-box-lines") \
  V(webkitBoxOrdinalGroup, "-webkit-box-ordinal-group") \
  V(webkitBoxOrient, "-webkit-box-orient") \
  V(webkitBoxPack, "-webkit-box-pack") \
  V(webkitBoxReflect, "-webkit-box-reflect") \
  V(webkitBoxShadow, "-webkit-box-shadow") \
  V(webkitBoxSnap, "-webkit-box-snap") \
  V(webkitColumnAxis, "-webkit-column-axis") \
  V(webkitColumnBreakAfter, "-webkit-column-break-after") \
  V(webkitColumnBreakBefore, "-webkit-column-break-before") \
  V(webkitColumnBreakInside, "-webkit-column-break-inside") \
  V(webkitColumnProgression, "-webkit-column-progression") \
  V(webkitCursorVisibility, "-webkit-cursor-visibility") \
  V(webkitFontSmoothing, "-webkit-font-smoothing") \
  V(webkitHyphenateCharacter, "-webkit-hyphenate-character") \
  V(webkitHyphenateLimitAfter, "-webkit-hyphenate-limit-after") \
  V(webkitHyphenateLimitBefore, "-webkit-hyphenate-limit-before") \
  V(webkitHyphenateLimitLines, "-webkit-hyphenate-limit-lines") \
  V(webkitHyphens, "-webkit-hyphens") \
  V(webkitInitialLetter, "-webkit-initial-letter") \
  V(webkitLineAlign, "-webkit-line-align") \
  V(webkitLineBoxContain, "-webkit-line-box-contain") \
  V(webkitLineClamp, "-webkit-line-clamp") \
  V(webkitLineGrid, "-webkit-line-grid") \
  V(webkitLineSnap, "-webkit-line-snap") \
  V(webkitLocale, "-webkit-locale") \
  V(webkitMask, "-webkit-mask") \
  V(webkitMaskBoxImage, "-webkit-mask-box-image") \
  V(webkitMaskBoxImageOutset, "-webkit-mask-box-image-outset") \
  V(webkitMaskBoxImageRepeat, "-webkit-mask-box-image-repeat") \
  V(webkitMaskBoxImageSlice, "-webkit-mask-box-image-slice") \
  V(webkitMaskBoxImageSource, "-webkit-mask-box-image-source") \
  V(webkitMaskBoxImageWidth, "-webkit-mask-box-image-width") \
  V(webkitMaskClip, "-webkit-mask-clip") \
  V(webkitMaskComposite, "-webkit-mask-composite") \
  V(webkitMaskPosition, "-webkit-mask-position") \
  V(webkitMaskPositionX, "-webkit-mask-position-x") \
  V(webkitMaskPositionY, "-webkit-mask-position-y") \
  V(webkitMaskSourceType, "-webkit-mask-source-type") \
  V(webkitNbspMode, "-webkit-nbsp-mode") \
  V(webkitPerspective, "-webkit-perspective") \
  V(webkitRtlOrdering, "-webkit-rtl-ordering") \
  V(webkitRubyPosition, "-webkit-ruby-position") \
  V(webkitTextCombine, "-webkit-text-combine") \
  V(webkitTextDecoration, "-webkit-text-decoration") \
  V(webkitTextDecorationsInEffect, "-webkit-text-decorations-in-effect") \
  V(webkitTextFillColor, "-webkit-text-fill-color") \
  V(webkitTextOrientation, "-webkit-text-orientation") \
  V(webkitTextSecurity, "-webkit-text-security") \
  V(webkitTextStroke, "-webkit-text-stroke") \
  V(webkitTextStrokeColor, "-webkit-text-stroke-color") \
  V(webkitTextStrokeWidth, "-webkit-text-stroke-width") \
  V(webkitTextZoom, "-webkit-text-zoom") \
  V(webkitUserDrag, "-webkit-user-drag") \
  V(webkitUserModify, "-webkit-user-modify") \
  V(webkitUserSelect, "-webkit-user-select") \
  V(accentColor, "accent-color") \
  V(alignContent, "align-content") \
  V(alignItems, "align-items") \
  V(alignSelf, "align-self") \
  V(alignTracks, "align-tracks") \
  V(alignmentBaseline, "alignment-baseline") \
  V(all, "all") \
  V(alt, "alt") \
  V(animation, "animation") \
  V(animationComposition, "animation-composition") \
  V(animationDelay, "animation-delay") \
  V(animationDirection, "animation-direction") \
  V(animationDuration, "animation-duration") \
  V(animationFillMode, "animation-fill-mode") \
  V(animationIterationCount, "animation-iteration-count") \
  V(animationName, "animation-name") \
  V(animationPlayState, "animation-play-state") \
  V(animationTimingFunction, "animation-timing-function") \
  V(appRegion, "app-region") \
  V(appearance, "appearance") \
  V(aspectRatio, "aspect-ratio") \
  V(backdropFilter, "backdrop-filter") \
  V(backfaceVisibility, "backface-visibility") \
  V(background, "background") \
  V(backgroundAttachment, "background-attachment") \
  V(backgroundBlendMode, "background-blend-mode") \
  V(backgroundClip, "background-clip") \
  V(backgroundColor, "background-color") \
  V(backgroundImage, "background-image") \
  V(backgroundOrigin, "background-origin") \
  V(backgroundPosition, "background-position") \
  V(backgroundPositionX, "background-position-x") \
  V(backgroundPositionY, "background-position-y") \
  V(backgroundRepeat, "background-repeat") \
  V(backgroundSize, "background-size") \
  V(baselineShift, "baseline-shift") \
  V(blockSize, "block-size") \
  V(border, "border") \
  V(borderBlock, "border-block") \
  V(borderBlockColor, "border-block-color") \
  V(borderBlockEnd, "border-block-end") \
  V(borderBlockEndColor, "border-block-end-color") \
  V(borderBlockEndStyle, "border-block-end-style") \
  V(borderBlockEndWidth, "border-block-end-width") \
  V(borderBlockStart, "border-block-start") \
  V(borderBlockStartColor, "border-block-start-color") \
  V(borderBlockStartStyle, "border-block-start-style") \
  V(borderBlockStartWidth, "border-block-start-width") \
  V(borderBlockStyle, "border-block-style") \
  V(borderBlockWidth, "border-block-width") \
  V(borderBottom, "border-bottom") \
  V(borderBottomColor, "border-bottom-color") \
  V(borderBottomLeftRadius, "border-bottom-left-radius") \
  V(borderBottomRightRadius, "border-bottom-right-radius") \
  V(borderBottomStyle, "border-bottom-style") \
  V(borderBottomWidth, "border-bottom-width") \
  V(borderBoundary, "border-boundary") \
  V(borderCollapse, "border-collapse") \
  V(borderColor, "border-color") \
  V(borderEndEndRadius, "border-end-end-radius") \
  V(borderEndStartRadius, "border-end-start-radius") \
  V(borderImage, "border-image") \
  V(borderImageOutset, "border-image-outset") \
  V(borderImageRepeat, "border-image-repeat") \
  V(borderImageSlice, "border-image-slice") \
  V(borderImageSource, "border-image-source") \
  V(borderImageWidth, "border-image-width") \
  V(borderInline, "border-inline") \
  V(borderInlineColor, "border-inline-color") \
  V(borderInlineEnd, "border-inline-end") \
  V(borderInlineEndColor, "border-inline-end-color") \
  V(borderInlineEndStyle, "border-inline-end-style") \
  V(borderInlineEndWidth, "border-inline-end-width") \
  V(borderInlineStart, "border-inline-start") \
  V(borderInlineStartColor, "border-inline-start-color") \
  V(borderInlineStartStyle, "border-inline-start-style") \
  V(borderInlineStartWidth, "border-inline-start-width") \
  V(borderInlineStyle, "border-inline-style") \
  V(borderInlineWidth, "border-inline-width") \
  V(borderLeft, "border-left") \
  V(borderLeftColor, "border-left-color") \
  V(borderLeftStyle, "border-left-style") \
  V(borderLeftWidth, "border-left-width") \
  V(borderRadius, "border-radius") \
  V(borderRight, "border-right") \
  V(borderRightColor, "border-right-color") \
  V(borderRightStyle, "border-right-style") \
  V(borderRightWidth, "border-right-width") \
  V(borderSpacing, "border-spacing") \
  V(borderStartEndRadius, "border-start-end-radius") \
  V(borderStartStartRadius, "border-start-start-radius") \
  V(borderStyle, "border-style") \
  V(borderTop, "border-top") \
  V(borderTopColor, "border-top-color") \
  V(borderTopLeftRadius, "border-top-left-radius") \
  V(borderTopRightRadius, "border-top-right-radius") \
  V(borderTopStyle, "border-top-style") \
  V(borderTopWidth, "border-top-width") \
  V(borderWidth, "border-width") \
  V(bottom, "bottom") \
  V(boxShadow, "box-shadow") \
  V(boxSizing, "box-sizing") \
  V(breakAfter, "break-after") \
  V(breakBefore, "break-before") \
  V(breakInside, "break-inside") \
  V(bufferedRendering, "buffered-rendering") \
  V(captionSide, "caption-side") \
  V(caretColor, "caret-color") \
  V(clear, "clear") \
  V(clip, "clip") \
  V(clipPath, "clip-path") \
  V(clipRule, "clip-rule") \
  V(color, "color") \
  V(colorInterpolation, "color-interpolation") \
  V(colorInterpolationFilters, "color-interpolation-filters") \
  V(colorScheme, "color-scheme") \
  V(columnCount, "column-count") \
  V(columnFill, "column-fill") \
  V(columnGap, "column-gap") \
  V(columnRule, "column-rule") \
  V(columnRuleColor, "column-rule-color") \
  V(columnRuleStyle, "column-rule-style") \
  V(columnRuleWidth, "column-rule-width") \
  V(columnSpan, "column-span") \
  V(columnWidth, "column-width") \
  V(columns, "columns") \
  V(contain, "contain") \
  V(containIntrinsicBlockSize, "contain-intrinsic-block-size") \
  V(containIntrinsicHeight, "contain-intrinsic-height") \
  V(containIntrinsicInlineSize, "contain-intrinsic-inline-size") \
  V(containIntrinsicSize, "contain-intrinsic-size") \
  V(containIntrinsicWidth, "contain-intrinsic-width") \
  V(container, "container") \
  V(containerName, "container-name") \
  V(containerType, "container-type") \
  V(content, "content") \
  V(contentVisibility, "content-visibility") \
  V(counterIncrement, "counter-increment") \
  V(counterReset, "counter-reset") \
  V(counterSet, "counter-set") \
  V(cursor, "cursor") \
  V(cx, "cx") \
  V(cy, "cy") \
  V(direction, "direction") \
  V(display, "display") \
  V(dominantBaseline, "dominant-baseline") \
  V(emptyCells, "empty-cells") \
  V(fill, "fill") \
  V(fillColor, "fill-color") \
  V(fillImage, "fill-image") \
  V(fillOpacity, "fill-opacity") \
  V(fillOrigin, "fill-origin") \
  V(fillPosition, "fill-position") \
  V(fillRule, "fill-rule") \
  V(filter, "filter") \
  V(flex, "flex") \
  V(flexBasis, "flex-basis") \
  V(flexDirection, "flex-direction") \
  V(flexFlow, "flex-flow") \
  V(flexGrow, "flex-grow") \
  V(flexShrink, "flex-shrink") \
  V(flexWrap, "flex-wrap") \
  V(cssFloat, "float") \
  V(floodColor, "flood-color") \
  V(floodOpacity, "flood-opacity") \
  V(font, "font") \
  V(fontFamily, "font-family") \
  V(fontFeatureSettings, "font-feature-settings") \
  V(fontKerning, "font-kerning") \
  V(fontOpticalSizing, "font-optical-sizing") \
  V(fontPalette, "font-palette") \
  V(fontSize, "font-size") \
  V(fontSizeAdjust, "font-size-adjust") \
  V(fontStretch, "font-stretch") \
  V(fontStyle, "font-style") \
  V(fontSynthesis, "font-synthesis") \
  V(fontSynthesisSmallCaps, "font-synthesis-small-caps") \
  V(fontSynthesisStyle, "font-synthesis-style") \
  V(fontSynthesisWeight, "font-synthesis-weight") \
  V(fontVariant, "font-variant") \
  V(fontVariantAlternates, "font-variant-alternates") \
  V(fontVariantCaps, "font-variant-caps") \
  V(fontVariantEastAsian, "font-variant-east-asian") \
  V(fontVariantLigatures, "font-variant-ligatures") \
  V(fontVariantNumeric, "font-variant-numeric") \
  V(fontVariantPosition, "font-variant-position") \
  V(fontVariationSettings, "font-variation-settings") \
  V(fontWeight, "font-weight") \
  V(gap, "gap") \
  V(glyphOrientationHorizontal, "glyph-orientation-horizontal") \
  V(glyphOrientationVertical, "glyph-orientation-vertical") \
  V(grid, "grid") \
  V(gridArea, "grid-area") \
  V(gridAutoColumns, "grid-auto-columns") \
  V(gridAutoFlow, "grid-auto-flow") \
  V(gridAutoRows, "grid-auto-rows") \
  V(gridColumn, "grid-column") \
  V(gridColumnEnd, "grid-column-end") \
  V(gridColumnStart, "grid-column-start") \
  V(gridRow, "grid-row") \
  V(gridRowEnd, "grid-row-end") \
  V(gridRowStart, "grid-row-start") \
  V(gridTemplate, "grid-template") \
  V(gridTemplateAreas, "grid-template-areas") \
  V(gridTemplateColumns, "grid-template-columns") \
  V(gridTemplateRows, "grid-template-rows") \
  V(hangingPunctuation, "hanging-punctuation") \
  V(height, "height") \
  V(imageOrientation, "image-orientation") \
  V(imageRendering, "image-rendering") \
  V(inlineSize, "inline-size") \
  V(inputSecurity, "input-security") \
  V(inset, "inset") \
  V(insetBlock, "inset-block") \
  V(insetBlockEnd, "inset-block-end") \
  V(insetBlockStart, "inset-block-start") \
  V(insetInline, "inset-inline") \
  V(insetInlineEnd, "inset-inline-end") \
  V(insetInlineStart, "inset-inline-start") \
  V(isolation, "isolation") \
  V(justifyContent, "justify-content") \
  V(justifyItems, "justify-items") \
  V(justifySelf, "justify-self") \
  V(justifyTracks, "justify-tracks") \
  V(kerning, "kerning") \
  V(leadingTrim, "leading-trim") \
  V(left, "left") \
  V(letterSpacing, "letter-spacing") \
  V(lightingColor, "lighting-color") \
  V(lineBreak, "line-break") \
  V(lineHeight, "line-height") \
  V(listStyle, "list-style") \
  V(listStyleImage, "list-style-image") \
  V(listStylePosition, "list-style-position") \
  V(listStyleType, "list-style-type") \
  V(margin, "margin") \
  V(marginBlock, "margin-block") \
  V(marginBlockEnd, "margin-block-end") \
  V(marginBlockStart, "margin-block-start") \
  V(marginBottom, "margin-bottom") \
  V(marginInline, "margin-inline") \
  V(marginInlineEnd, "margin-inline-end") \
  V(marginInlineStart, "margin-inline-start") \
  V(marginLeft, "margin-left") \
  V(marginRight, "margin-right") \
  V(marginTop, "margin-top") \
  V(marginTrim, "margin-trim") \
  V(marker, "marker") \
  V(markerEnd, "marker-end") \
  V(markerMid, "marker-mid") \
  V(markerStart, "marker-start") \
  V(mask, "mask") \
  V(maskClip, "mask-clip") \
  V(maskComposite, "mask-composite") \
  V(maskImage, "mask-image") \
  V(maskMode, "mask-mode") \
  V(maskOrigin, "mask-origin") \
  V(maskPosition, "mask-position") \
  V(maskRepeat, "mask-repeat") \
  V(maskSize, "mask-size") \
  V(maskType, "mask-type") \
  V(masonryAutoFlow, "masonry-auto-flow") \
  V(mathStyle, "math-style") \
  V(maxBlockSize, "max-block-size") \
  V(maxHeight, "max-height") \
  V(maxInlineSize, "max-inline-size") \
  V(maxWidth, "max-width") \
  V(minBlockSize, "min-block-size") \
  V(minHeight, "min-height") \
  V(minInlineSize, "min-inline-size") \
  V(minWidth, "min-width") \
  V(mixBlendMode, "mix-blend-mode") \
  V(objectFit, "object-fit") \
  V(objectPosition, "object-position") \
  V(offset, "offset") \
  V(offsetAnchor, "offset-anchor") \
  V(offsetDistance, "offset-distance") \
  V(offsetPath, "offset-path") \
  V(offsetPosition, "offset-position") \
  V(offsetRotate, "offset-rotate") \
  V(opacity, "opacity") \
  V(order, "order") \
  V(orphans, "orphans") \
  V(outline, "outline") \
  V(outlineColor, "outline-color") \
  V(outlineOffset, "outline-offset") \
  V(outlineStyle, "outline-style") \
  V(outlineWidth, "outline-width") \
  V(overflow, "overflow") \
  V(overflowAnchor, "overflow-anchor") \
  V(overflowWrap, "overflow-wrap") \
  V(overflowX, "overflow-x") \
  V(overflowY, "overflow-y") \
  V(overscrollBehavior, "overscroll-behavior") \
  V(overscrollBehaviorBlock, "overscroll-behavior-block") \
  V(overscrollBehaviorInline, "overscroll-behavior-inline") \
  V(overscrollBehaviorX, "overscroll-behavior-x") \
  V(overscrollBehaviorY, "overscroll-behavior-y") \
  V(padding, "padding") \
  V(paddingBlock, "padding-block") \
  V(paddingBlockEnd, "padding-block-end") \
  V(paddingBlockStart, "padding-block-start") \
  V(paddingBottom, "padding-bottom") \
  V(paddingInline, "padding-inline") \
  V(paddingInlineEnd, "padding-inline-end") \
  V(paddingInlineStart, "padding-inline-start") \
  V(paddingLeft, "padding-left") \
  V(paddingRight, "padding-right") \
  V(paddingTop, "padding-top") \
  V(page, "page") \
  V(pageBreakAfter, "page-break-after") \
  V(pageBreakBefore, "page-break-before") \
  V(pageBreakInside, "page-break-inside") \
  V(paintOrder, "paint-order") \
  V(perspective, "perspective") \
  V(perspectiveOrigin, "perspective-origin") \
  V(perspectiveOriginX, "perspective-origin-x") \
  V(perspectiveOriginY, "perspective-origin-y") \
  V(placeContent, "place-content") \
  V(placeItems, "place-items") \
  V(placeSelf, "place-self") \
  V(pointerEvents, "pointer-events") \
  V(position, "position") \
  V(printColorAdjust, "print-color-adjust") \
  V(quotes, "quotes") \
  V(r, "r") \
  V(resize, "resize") \
  V(right, "right") \
  V(rotate, "rotate") \
  V(rowGap, "row-gap") \
  V(rx, "rx") \
  V(ry, "ry") \
  V(scale, "scale") \
  V(scrollBehavior, "scroll-behavior") \
  V(scrollMargin, "scroll-margin") \
  V(scrollMarginBlock, "scroll-margin-block") \
  V(scrollMarginBlockEnd, "scroll-margin-block-end") \
  V(scrollMarginBlockStart, "scroll-margin-block-start") \
  V(scrollMarginBottom, "scroll-margin-bottom") \
  V(scrollMarginInline, "scroll-margin-inline") \
  V(scrollMarginInlineEnd, "scroll-margin-inline-end") \
  V(scrollMarginInlineStart, "scroll-margin-inline-start") \
  V(scrollMarginLeft, "scroll-margin-left") \
  V(scrollMarginRight, "scroll-margin-right") \
  V(scrollMarginTop, "scroll-margin-top") \
  V(scrollPadding, "scroll-padding") \
  V(scrollPaddingBlock, "scroll-padding-block") \
  V(scrollPaddingBlockEnd, "scroll-padding-block-end") \
  V(scrollPaddingBlockStart, "scroll-padding-block-start") \
  V(scrollPaddingBottom, "scroll-padding-bottom") \
  V(scrollPaddingInline, "scroll-padding-inline") \
  V(scrollPaddingInlineEnd, "scroll-padding-inline-end") \
  V(scrollPaddingInlineStart, "scroll-padding-inline-start") \
  V(scrollPaddingLeft, "scroll-padding-left") \
  V(scrollPaddingRight, "scroll-padding-right") \
  V(scrollPaddingTop, "scroll-padding-top") \
  V(scrollSnapAlign, "scroll-snap-align") \
  V(scrollSnapStop, "scroll-snap-stop") \
  V(scrollSnapType, "scroll-snap-type") \
  V(shapeImageThreshold, "shape-image-threshold") \
  V(shapeMargin, "shape-margin") \
  V(shapeOutside, "shape-outside") \
  V(shapeRendering, "shape-rendering") \
  V(size, "size") \
  V(speakAs, "speak-as") \
  V(stopColor, "stop-color") \
  V(stopOpacity, "stop-opacity") \
  V(stroke, "stroke") \
  V(strokeColor, "stroke-color") \
  V(strokeDasharray, "stroke-dasharray") \
  V(strokeDashoffset, "stroke-dashoffset") \
  V(strokeLinecap, "stroke-linecap") \
  V(strokeLinejoin, "stroke-linejoin") \
  V(strokeMiterlimit, "stroke-miterlimit") \
  V(strokeOpacity, "stroke-opacity") \
  V(strokeWidth, "stroke-width") \
  V(tabSize, "tab-size") \
  V(tableLayout, "table-layout") \
  V(textAlign, "text-align") \
  V(textAlignLast, "text-align-last") \
  V(textAnchor, "text-anchor") \
  V(textCombineUpright, "text-combine-upright") \
  V(textDecoration, "text-decoration") \
  V(textDecorationColor, "text-decoration-color") \
  V(textDecorationLine, "text-decoration-line") \
  V(textDecorationSkip, "text-decoration-skip") \
  V(textDecorationSkipInk, "text-decoration-skip-ink") \
  V(textDecorationStyle, "text-decoration-style") \
  V(textDecorationThickness, "text-decoration-thickness") \
  V(textEdge, "text-edge") \
  V(textEmphasis, "text-emphasis") \
  V(textEmphasisColor, "text-emphasis-color") \
  V(textEmphasisPosition, "text-emphasis-position") \
  V(textEmphasisStyle, "text-emphasis-style") \
  V(textGroupAlign, "text-group-align") \
  V(textIndent, "text-indent") \
  V(textJustify, "text-justify") \
  V(textLineThrough, "text-line-through") \
  V(textLineThroughColor, "text-line-through-color") \
  V(textLineThroughMode, "text-line-through-mode") \
  V(textLineThroughStyle, "text-line-through-style") \
  V(textLineThroughWidth, "text-line-through-width") \
  V(textOrientation, "text-orientation") \
  V(textOverflow, "text-overflow") \
  V(textOverline, "text-overline") \
  V(textOverlineColor, "text-overline-color") \
  V(textOverlineMode, "text-overline-mode") \
  V(textOverlineStyle, "text-overline-style") \
  V(textOverlineWidth, "text-overline-width") \
  V(textRendering, "text-rendering") \
  V(textShadow, "text-shadow") \
  V(textTransform, "text-transform") \
  V(textUnderline, "text-underline") \
  V(textUnderlineColor, "text-underline-color") \
  V(textUnderlineMode, "text-underline-mode") \
  V(textUnderlineOffset, "text-underline-offset") \
  V(textUnderlinePosition, "text-underline-position") \
  V(textUnderlineStyle, "text-underline-style") \
  V(textUnderlineWidth, "text-underline-width") \
  V(textWrap, "text-wrap") \
  V(top, "top") \
  V(touchAction, "touch-action") \
  V(transform, "transform") \
  V(transformBox, "transform-box") \
  V(transformOrigin, "transform-origin") \
  V(transformOriginX, "transform-origin-x") \
  V(transformOriginY, "transform-origin-y") \
  V(transformOriginZ, "transform-origin-z") \
  V(transformStyle, "transform-style") \
  V(transition, "transition") \
  V(transitionDelay, "transition-delay") \
  V(transitionDuration, "transition-duration") \
  V(transitionProperty, "transition-property") \
  V(transitionTimingFunction, "transition-timing-function") \
  V(translate, "translate") \
  V(unicodeBidi, "unicode-bidi") \
  V(vectorEffect, "vector-effect") \
  V(verticalAlign, "vertical-align") \
  V(visibility, "visibility") \
  V(whiteSpace, "white-space") \
  V(widows, "widows") \
  V(width, "width") \
  V(willChange, "will-change") \
  V(wordBreak, "word-break") \
  V(wordSpacing, "word-spacing") \
  V(writingMode, "writing-mode") \
  V(x, "x") \
  V(y, "y") \
  V(zIndex, "z-index") \
  V(zoom, "zoom") \
  /* end */

// V(aliasIdentifier, aliasCssName, canonicalIdentifier)
#define ULDOM_FOR_EACH_CSS_PROPERTY_ALIAS(V) \
  V(epubCaptionSide, "-epub-caption-side", captionSide) \
  V(epubHyphens, "-epub-hyphens", webkitHyphens) \
  V(epubTextCombine, "-epub-text-combine", webkitTextCombine) \
  V(epubTextEmphasis, "-epub-text-emphasis", textEmphasis) \
  V(epubTextEmphasisColor, "-epub-text-emphasis-color", textEmphasisColor) \
  V(epubTextEmphasisStyle, "-epub-text-emphasis-style", textEmphasisStyle) \
  V(epubTextOrientation, "-epub-text-orientation", webkitTextOrientation) \
  V(epubTextTransform, "-epub-text-transform", textTransform) \
  V(epubWordBreak, "-epub-word-break", wordBreak) \
  V(epubWritingMode, "-epub-writing-mode", writingMode) \
  V(webkitAlignContent, "-webkit-align-content", alignContent) \
  V(webkitAlignItems, "-webkit-align-items", alignItems) \
  V(webkitAlignSelf, "-webkit-align-self", alignSelf) \
  V(webkitAlt, "-webkit-alt", alt) \
  V(webkitAnimation, "-webkit-animation", animation) \
  V(webkitAnimationDelay, "-webkit-animation-delay", animationDelay) \
  V(webkitAnimationDirection, "-webkit-animation-direction", animationDirection) \
  V(webkitAnimationDuration, "-webkit-animation-duration", animationDuration) \
  V(webkitAnimationFillMode, "-webkit-animation-fill-mode", animationFillMode) \
  V(webkitAnimationIterationCount, "-webkit-animation-iteration-count", animationIterationCount) \
  V(webkitAnimationName, "-webkit-animation-name", animationName) \
  V(webkitAnimationPlayState, "-webkit-animation-play-state", animationPlayState) \
  V(webkitAnimationTimingFunction, "-webkit-animation-timing-function", animationTimingFunction) \
  V(webkitAppRegion, "-webkit-app-region", appRegion) \
  V(webkitAppearance, "-webkit-appearance", appearance) \
  V(webkitBackdropFilter, "-webkit-backdrop-filter", backdropFilter) \
  V(webkitBackfaceVisibility, "-webkit-backface-visibility", backfaceVisibility) \
  V(webkitBorderAfter, "-webkit-border-after", borderBlockEnd) \
  V(webkitBorderAfterColor, "-webkit-border-after-color", borderBlockEndColor) \
  V(webkitBorderAfterStyle, "-webkit-border-after-style", borderBlockEndStyle) \
  V(webkitBorderAfterWidth, "-webkit-border-after-width", borderBlockEndWidth) \
  V(webkitBorderBefore, "-webkit-border-before", borderBlockStart) \
  V(webkitBorderBeforeColor, "-webkit-border-before-color", borderBlockStartColor) \
  V(webkitBorderBeforeStyle, "-webkit-border-before-style", borderBlockStartStyle) \
  V(webkitBorderBeforeWidth, "-webkit-border-before-width", borderBlockStartWidth) \
  V(webkitBorderBottomLeftRadius, "-webkit-border-bottom-left-radius", borderBottomLeftRadius) \
  V(webkitBorderBottomRightRadius, "-webkit-border-bottom-right-radius", borderBottomRightRadius) \
  V(webkitBorderEnd, "-webkit-border-end", borderInlineEnd) \
  V(webkitBorderEndColor, "-webkit-border-end-color", borderInlineEndColor) \
  V(webkitBorderEndStyle, "-webkit-border-end-style", borderInlineEndStyle) \
  V(webkitBorderEndWidth, "-webkit-border-end-width", borderInlineEndWidth) \
  V(webkitBorderStart, "-webkit-border-start", borderInlineStart) \
  V(webkitBorderStartColor, "-webkit-border-start-color", borderInlineStartColor) \
  V(webkitBorderStartStyle, "-webkit-border-start-style", borderInlineStartStyle) \
  V(webkitBorderStartWidth, "-webkit-border-start-width", borderInlineStartWidth) \
  V(webkitBorderTopLeftRadius, "-webkit-border-top-left-radius", borderTopLeftRadius) \
  V(webkitBorderTopRightRadius, "-webkit-border-top-right-radius", borderTopRightRadius) \
  V(webkitBoxSizing, "-webkit-box-sizing", boxSizing) \
  V(webkitClipPath, "-webkit-clip-path", clipPath) \
  V(webkitColumnCount, "-webkit-column-count", columnCount) \
  V(webkitColumnFill, "-webkit-column-fill", columnFill) \
  V(webkitColumnGap, "-webkit-column-gap", columnGap) \
  V(webkitColumnRule, "-webkit-column-rule", columnRule) \
  V(webkitColumnRuleColor, "-webkit-column-rule-color", columnRuleColor) \
  V(webkitColumnRuleStyle, "-webkit-column-rule-style", columnRuleStyle) \
  V(webkitColumnRuleWidth, "-webkit-column-rule-width", columnRuleWidth) \
  V(webkitColumnSpan, "-webkit-column-span", columnSpan) \
  V(webkitColumnWidth, "-webkit-column-width", columnWidth) \
  V(webkitColumns, "-webkit-columns", columns) \
  V(webkitFilter, "-webkit-filter", filter) \
  V(webkitFlex, "-webkit-flex", flex) \
  V(webkitFlexBasis, "-webkit-flex-basis", flexBasis) \
  V(webkitFlexDirection, "-webkit-flex-direction", flexDirection) \
  V(webkitFlexFlow, "-webkit-flex-flow", flexFlow) \
  V(webkitFlexGrow, "-webkit-flex-grow", flexGrow) \
  V(webkitFlexShrink, "-webkit-flex-shrink", flexShrink) \
  V(webkitFlexWrap, "-webkit-flex-wrap", flexWrap) \
  V(webkitFontKerning, "-webkit-font-kerning", fontKerning) \
  V(webkitJustifyContent, "-webkit-justify-content", justifyContent) \
  V(webkitJustifyItems, "-webkit-justify-items", justifyItems) \
  V(webkitLineBreak, "-webkit-line-break", lineBreak) \
  V(webkitLogicalHeight, "-webkit-logical-height", blockSize) \
  V(webkitLogicalWidth, "-webkit-logical-width", inlineSize) \
  V(webkitMarginAfter, "-webkit-margin-after", marginBlockEnd) \
  V(webkitMarginBefore, "-webkit-margin-before", marginBlockStart) \
  V(webkitMarginEnd, "-webkit-margin-end", marginInlineEnd) \
  V(webkitMarginStart, "-webkit-margin-start", marginInlineStart) \
  V(webkitMaskImage, "-webkit-mask-image", maskImage) \
  V(webkitMaskOrigin, "-webkit-mask-origin", maskOrigin) \
  V(webkitMaskRepeat, "-webkit-mask-repeat", maskRepeat) \
  V(webkitMaskSize, "-webkit-mask-size", maskSize) \
  V(webkitMaxLogicalHeight, "-webkit-max-logical-height", maxBlockSize) \
  V(webkitMaxLogicalWidth, "-webkit-max-logical-width", maxInlineSize) \
  V(webkitMinLogicalHeight, "-webkit-min-logical-height", minBlockSize) \
  V(webkitMinLogicalWidth, "-webkit-min-logical-width", minInlineSize) \
  V(webkitOpacity, "-webkit-opacity", opacity) \
  V(webkitOrder, "-webkit-order", order) \
  V(webkitPaddingAfter, "-webkit-padding-after", paddingBlockEnd) \
  V(webkitPaddingBefore, "-webkit-padding-before", paddingBlockStart) \
  V(webkitPaddingEnd, "-webkit-padding-end", paddingInlineEnd) \
  V(webkitPaddingStart, "-webkit-padding-start", paddingInlineStart) \
  V(webkitPerspectiveOrigin, "-webkit-perspective-origin", perspectiveOrigin) \
  V(webkitPerspectiveOriginX, "-webkit-perspective-origin-x", perspectiveOriginX) \
  V(webkitPerspectiveOriginY, "-webkit-perspective-origin-y", perspectiveOriginY) \
  V(webkitPrintColorAdjust, "-webkit-print-color-adjust", printColorAdjust) \
  V(webkitShapeImageThreshold, "-webkit-shape-image-threshold", shapeImageThreshold) \
  V(webkitShapeMargin, "-webkit-shape-margin", shapeMargin) \
  V(webkitShapeOutside, "-webkit-shape-outside", shapeOutside) \
  V(webkitTextDecorationColor, "-webkit-text-decoration-color", textDecorationColor) \
  V(webkitTextDecorationLine, "-webkit-text-decoration-line", textDecorationLine) \
  V(webkitTextDecorationSkip, "-webkit-text-decoration-skip", textDecorationSkip) \
  V(webkitTextDecorationStyle, "-webkit-text-decoration-style", textDecorationStyle) \
  V(webkitTextEmphasis, "-webkit-text-emphasis", textEmphasis) \
  V(webkitTextEmphasisColor, "-webkit-text-emphasis-color", textEmphasisColor) \
  V(webkitTextEmphasisPosition, "-webkit-text-emphasis-position", textEmphasisPosition) \
  V(webkitTextEmphasisStyle, "-webkit-text-emphasis-style", textEmphasisStyle) \
  V(webkitTextUnderlinePosition, "-webkit-text-underline-position", textUnderlinePosition) \
  V(webkitTransform, "-webkit-transform", transform) \
  V(webkitTransformOrigin, "-webkit-transform-origin", transformOrigin) \
  V(webkitTransformOriginX, "-webkit-transform-origin-x", transformOriginX) \
  V(webkitTransformOriginY, "-webkit-transform-origin-y", transformOriginY) \
  V(webkitTransformOriginZ, "-webkit-transform-origin-z", transformOriginZ) \
  V(webkitTransformStyle, "-webkit-transform-style", transformStyle) \
  V(webkitTransition, "-webkit-transition", transition) \
  V(webkitTransitionDelay, "-webkit-transition-delay", transitionDelay) \
  V(webkitTransitionDuration, "-webkit-transition-duration", transitionDuration) \
  V(webkitTransitionProperty, "-webkit-transition-property", transitionProperty) \
  V(webkitTransitionTimingFunction, "-webkit-transition-timing-function", transitionTimingFunction) \
  V(webkitWritingMode, "-webkit-writing-mode", writingMode) \
  V(colorAdjust, "color-adjust", printColorAdjust) \
  V(gridColumnGap, "grid-column-gap", columnGap) \
  V(gridGap, "grid-gap", gap) \
  V(gridRowGap, "grid-row-gap", rowGap) \
  V(scrollSnapMargin, "scroll-snap-margin", scrollMargin) \
  V(scrollSnapMarginBottom, "scroll-snap-margin-bottom", scrollMarginBottom) \
  V(scrollSnapMarginLeft, "scroll-snap-margin-left", scrollMarginLeft) \
  V(scrollSnapMarginRight, "scroll-snap-margin-right", scrollMarginRight) \
  V(scrollSnapMarginTop, "scroll-snap-margin-top", scrollMarginTop) \
  V(supportedColorSchemes, "supported-color-schemes", colorScheme) \
  V(wordWrap, "word-wrap", overflowWrap) \
  /* end */

// V(identifier, cssName) for every property and alias row, sorted by identifier (the
// camelCase-to-CSS-name lookup binary-searches it).
#define ULDOM_FOR_EACH_CSS_IDENTIFIER_SORTED(V) \
  V(accentColor, "accent-color") \
  V(alignContent, "align-content") \
  V(alignItems, "align-items") \
  V(alignSelf, "align-self") \
  V(alignTracks, "align-tracks") \
  V(alignmentBaseline, "alignment-baseline") \
  V(all, "all") \
  V(alt, "alt") \
  V(animation, "animation") \
  V(animationComposition, "animation-composition") \
  V(animationDelay, "animation-delay") \
  V(animationDirection, "animation-direction") \
  V(animationDuration, "animation-duration") \
  V(animationFillMode, "animation-fill-mode") \
  V(animationIterationCount, "animation-iteration-count") \
  V(animationName, "animation-name") \
  V(animationPlayState, "animation-play-state") \
  V(animationTimingFunction, "animation-timing-function") \
  V(appRegion, "app-region") \
  V(appearance, "appearance") \
  V(aspectRatio, "aspect-ratio") \
  V(backdropFilter, "backdrop-filter") \
  V(backfaceVisibility, "backface-visibility") \
  V(background, "background") \
  V(backgroundAttachment, "background-attachment") \
  V(backgroundBlendMode, "background-blend-mode") \
  V(backgroundClip, "background-clip") \
  V(backgroundColor, "background-color") \
  V(backgroundImage, "background-image") \
  V(backgroundOrigin, "background-origin") \
  V(backgroundPosition, "background-position") \
  V(backgroundPositionX, "background-position-x") \
  V(backgroundPositionY, "background-position-y") \
  V(backgroundRepeat, "background-repeat") \
  V(backgroundSize, "background-size") \
  V(baselineShift, "baseline-shift") \
  V(blockSize, "block-size") \
  V(border, "border") \
  V(borderBlock, "border-block") \
  V(borderBlockColor, "border-block-color") \
  V(borderBlockEnd, "border-block-end") \
  V(borderBlockEndColor, "border-block-end-color") \
  V(borderBlockEndStyle, "border-block-end-style") \
  V(borderBlockEndWidth, "border-block-end-width") \
  V(borderBlockStart, "border-block-start") \
  V(borderBlockStartColor, "border-block-start-color") \
  V(borderBlockStartStyle, "border-block-start-style") \
  V(borderBlockStartWidth, "border-block-start-width") \
  V(borderBlockStyle, "border-block-style") \
  V(borderBlockWidth, "border-block-width") \
  V(borderBottom, "border-bottom") \
  V(borderBottomColor, "border-bottom-color") \
  V(borderBottomLeftRadius, "border-bottom-left-radius") \
  V(borderBottomRightRadius, "border-bottom-right-radius") \
  V(borderBottomStyle, "border-bottom-style") \
  V(borderBottomWidth, "border-bottom-width") \
  V(borderBoundary, "border-boundary") \
  V(borderCollapse, "border-collapse") \
  V(borderColor, "border-color") \
  V(borderEndEndRadius, "border-end-end-radius") \
  V(borderEndStartRadius, "border-end-start-radius") \
  V(borderImage, "border-image") \
  V(borderImageOutset, "border-image-outset") \
  V(borderImageRepeat, "border-image-repeat") \
  V(borderImageSlice, "border-image-slice") \
  V(borderImageSource, "border-image-source") \
  V(borderImageWidth, "border-image-width") \
  V(borderInline, "border-inline") \
  V(borderInlineColor, "border-inline-color") \
  V(borderInlineEnd, "border-inline-end") \
  V(borderInlineEndColor, "border-inline-end-color") \
  V(borderInlineEndStyle, "border-inline-end-style") \
  V(borderInlineEndWidth, "border-inline-end-width") \
  V(borderInlineStart, "border-inline-start") \
  V(borderInlineStartColor, "border-inline-start-color") \
  V(borderInlineStartStyle, "border-inline-start-style") \
  V(borderInlineStartWidth, "border-inline-start-width") \
  V(borderInlineStyle, "border-inline-style") \
  V(borderInlineWidth, "border-inline-width") \
  V(borderLeft, "border-left") \
  V(borderLeftColor, "border-left-color") \
  V(borderLeftStyle, "border-left-style") \
  V(borderLeftWidth, "border-left-width") \
  V(borderRadius, "border-radius") \
  V(borderRight, "border-right") \
  V(borderRightColor, "border-right-color") \
  V(borderRightStyle, "border-right-style") \
  V(borderRightWidth, "border-right-width") \
  V(borderSpacing, "border-spacing") \
  V(borderStartEndRadius, "border-start-end-radius") \
  V(borderStartStartRadius, "border-start-start-radius") \
  V(borderStyle, "border-style") \
  V(borderTop, "border-top") \
  V(borderTopColor, "border-top-color") \
  V(borderTopLeftRadius, "border-top-left-radius") \
  V(borderTopRightRadius, "border-top-right-radius") \
  V(borderTopStyle, "border-top-style") \
  V(borderTopWidth, "border-top-width") \
  V(borderWidth, "border-width") \
  V(bottom, "bottom") \
  V(boxShadow, "box-shadow") \
  V(boxSizing, "box-sizing") \
  V(breakAfter, "break-after") \
  V(breakBefore, "break-before") \
  V(breakInside, "break-inside") \
  V(bufferedRendering, "buffered-rendering") \
  V(captionSide, "caption-side") \
  V(caretColor, "caret-color") \
  V(clear, "clear") \
  V(clip, "clip") \
  V(clipPath, "clip-path") \
  V(clipRule, "clip-rule") \
  V(color, "color") \
  V(colorAdjust, "color-adjust") \
  V(colorInterpolation, "color-interpolation") \
  V(colorInterpolationFilters, "color-interpolation-filters") \
  V(colorScheme, "color-scheme") \
  V(columnCount, "column-count") \
  V(columnFill, "column-fill") \
  V(columnGap, "column-gap") \
  V(columnRule, "column-rule") \
  V(columnRuleColor, "column-rule-color") \
  V(columnRuleStyle, "column-rule-style") \
  V(columnRuleWidth, "column-rule-width") \
  V(columnSpan, "column-span") \
  V(columnWidth, "column-width") \
  V(columns, "columns") \
  V(contain, "contain") \
  V(containIntrinsicBlockSize, "contain-intrinsic-block-size") \
  V(containIntrinsicHeight, "contain-intrinsic-height") \
  V(containIntrinsicInlineSize, "contain-intrinsic-inline-size") \
  V(containIntrinsicSize, "contain-intrinsic-size") \
  V(containIntrinsicWidth, "contain-intrinsic-width") \
  V(container, "container") \
  V(containerName, "container-name") \
  V(containerType, "container-type") \
  V(content, "content") \
  V(contentVisibility, "content-visibility") \
  V(counterIncrement, "counter-increment") \
  V(counterReset, "counter-reset") \
  V(counterSet, "counter-set") \
  V(cssFloat, "float") \
  V(cursor, "cursor") \
  V(cx, "cx") \
  V(cy, "cy") \
  V(direction, "direction") \
  V(display, "display") \
  V(dominantBaseline, "dominant-baseline") \
  V(emptyCells, "empty-cells") \
  V(epubCaptionSide, "-epub-caption-side") \
  V(epubHyphens, "-epub-hyphens") \
  V(epubTextCombine, "-epub-text-combine") \
  V(epubTextEmphasis, "-epub-text-emphasis") \
  V(epubTextEmphasisColor, "-epub-text-emphasis-color") \
  V(epubTextEmphasisStyle, "-epub-text-emphasis-style") \
  V(epubTextOrientation, "-epub-text-orientation") \
  V(epubTextTransform, "-epub-text-transform") \
  V(epubWordBreak, "-epub-word-break") \
  V(epubWritingMode, "-epub-writing-mode") \
  V(fill, "fill") \
  V(fillColor, "fill-color") \
  V(fillImage, "fill-image") \
  V(fillOpacity, "fill-opacity") \
  V(fillOrigin, "fill-origin") \
  V(fillPosition, "fill-position") \
  V(fillRule, "fill-rule") \
  V(filter, "filter") \
  V(flex, "flex") \
  V(flexBasis, "flex-basis") \
  V(flexDirection, "flex-direction") \
  V(flexFlow, "flex-flow") \
  V(flexGrow, "flex-grow") \
  V(flexShrink, "flex-shrink") \
  V(flexWrap, "flex-wrap") \
  V(floodColor, "flood-color") \
  V(floodOpacity, "flood-opacity") \
  V(font, "font") \
  V(fontFamily, "font-family") \
  V(fontFeatureSettings, "font-feature-settings") \
  V(fontKerning, "font-kerning") \
  V(fontOpticalSizing, "font-optical-sizing") \
  V(fontPalette, "font-palette") \
  V(fontSize, "font-size") \
  V(fontSizeAdjust, "font-size-adjust") \
  V(fontStretch, "font-stretch") \
  V(fontStyle, "font-style") \
  V(fontSynthesis, "font-synthesis") \
  V(fontSynthesisSmallCaps, "font-synthesis-small-caps") \
  V(fontSynthesisStyle, "font-synthesis-style") \
  V(fontSynthesisWeight, "font-synthesis-weight") \
  V(fontVariant, "font-variant") \
  V(fontVariantAlternates, "font-variant-alternates") \
  V(fontVariantCaps, "font-variant-caps") \
  V(fontVariantEastAsian, "font-variant-east-asian") \
  V(fontVariantLigatures, "font-variant-ligatures") \
  V(fontVariantNumeric, "font-variant-numeric") \
  V(fontVariantPosition, "font-variant-position") \
  V(fontVariationSettings, "font-variation-settings") \
  V(fontWeight, "font-weight") \
  V(gap, "gap") \
  V(glyphOrientationHorizontal, "glyph-orientation-horizontal") \
  V(glyphOrientationVertical, "glyph-orientation-vertical") \
  V(grid, "grid") \
  V(gridArea, "grid-area") \
  V(gridAutoColumns, "grid-auto-columns") \
  V(gridAutoFlow, "grid-auto-flow") \
  V(gridAutoRows, "grid-auto-rows") \
  V(gridColumn, "grid-column") \
  V(gridColumnEnd, "grid-column-end") \
  V(gridColumnGap, "grid-column-gap") \
  V(gridColumnStart, "grid-column-start") \
  V(gridGap, "grid-gap") \
  V(gridRow, "grid-row") \
  V(gridRowEnd, "grid-row-end") \
  V(gridRowGap, "grid-row-gap") \
  V(gridRowStart, "grid-row-start") \
  V(gridTemplate, "grid-template") \
  V(gridTemplateAreas, "grid-template-areas") \
  V(gridTemplateColumns, "grid-template-columns") \
  V(gridTemplateRows, "grid-template-rows") \
  V(hangingPunctuation, "hanging-punctuation") \
  V(height, "height") \
  V(imageOrientation, "image-orientation") \
  V(imageRendering, "image-rendering") \
  V(inlineSize, "inline-size") \
  V(inputSecurity, "input-security") \
  V(inset, "inset") \
  V(insetBlock, "inset-block") \
  V(insetBlockEnd, "inset-block-end") \
  V(insetBlockStart, "inset-block-start") \
  V(insetInline, "inset-inline") \
  V(insetInlineEnd, "inset-inline-end") \
  V(insetInlineStart, "inset-inline-start") \
  V(isolation, "isolation") \
  V(justifyContent, "justify-content") \
  V(justifyItems, "justify-items") \
  V(justifySelf, "justify-self") \
  V(justifyTracks, "justify-tracks") \
  V(kerning, "kerning") \
  V(leadingTrim, "leading-trim") \
  V(left, "left") \
  V(letterSpacing, "letter-spacing") \
  V(lightingColor, "lighting-color") \
  V(lineBreak, "line-break") \
  V(lineHeight, "line-height") \
  V(listStyle, "list-style") \
  V(listStyleImage, "list-style-image") \
  V(listStylePosition, "list-style-position") \
  V(listStyleType, "list-style-type") \
  V(margin, "margin") \
  V(marginBlock, "margin-block") \
  V(marginBlockEnd, "margin-block-end") \
  V(marginBlockStart, "margin-block-start") \
  V(marginBottom, "margin-bottom") \
  V(marginInline, "margin-inline") \
  V(marginInlineEnd, "margin-inline-end") \
  V(marginInlineStart, "margin-inline-start") \
  V(marginLeft, "margin-left") \
  V(marginRight, "margin-right") \
  V(marginTop, "margin-top") \
  V(marginTrim, "margin-trim") \
  V(marker, "marker") \
  V(markerEnd, "marker-end") \
  V(markerMid, "marker-mid") \
  V(markerStart, "marker-start") \
  V(mask, "mask") \
  V(maskClip, "mask-clip") \
  V(maskComposite, "mask-composite") \
  V(maskImage, "mask-image") \
  V(maskMode, "mask-mode") \
  V(maskOrigin, "mask-origin") \
  V(maskPosition, "mask-position") \
  V(maskRepeat, "mask-repeat") \
  V(maskSize, "mask-size") \
  V(maskType, "mask-type") \
  V(masonryAutoFlow, "masonry-auto-flow") \
  V(mathStyle, "math-style") \
  V(maxBlockSize, "max-block-size") \
  V(maxHeight, "max-height") \
  V(maxInlineSize, "max-inline-size") \
  V(maxWidth, "max-width") \
  V(minBlockSize, "min-block-size") \
  V(minHeight, "min-height") \
  V(minInlineSize, "min-inline-size") \
  V(minWidth, "min-width") \
  V(mixBlendMode, "mix-blend-mode") \
  V(objectFit, "object-fit") \
  V(objectPosition, "object-position") \
  V(offset, "offset") \
  V(offsetAnchor, "offset-anchor") \
  V(offsetDistance, "offset-distance") \
  V(offsetPath, "offset-path") \
  V(offsetPosition, "offset-position") \
  V(offsetRotate, "offset-rotate") \
  V(opacity, "opacity") \
  V(order, "order") \
  V(orphans, "orphans") \
  V(outline, "outline") \
  V(outlineColor, "outline-color") \
  V(outlineOffset, "outline-offset") \
  V(outlineStyle, "outline-style") \
  V(outlineWidth, "outline-width") \
  V(overflow, "overflow") \
  V(overflowAnchor, "overflow-anchor") \
  V(overflowWrap, "overflow-wrap") \
  V(overflowX, "overflow-x") \
  V(overflowY, "overflow-y") \
  V(overscrollBehavior, "overscroll-behavior") \
  V(overscrollBehaviorBlock, "overscroll-behavior-block") \
  V(overscrollBehaviorInline, "overscroll-behavior-inline") \
  V(overscrollBehaviorX, "overscroll-behavior-x") \
  V(overscrollBehaviorY, "overscroll-behavior-y") \
  V(padding, "padding") \
  V(paddingBlock, "padding-block") \
  V(paddingBlockEnd, "padding-block-end") \
  V(paddingBlockStart, "padding-block-start") \
  V(paddingBottom, "padding-bottom") \
  V(paddingInline, "padding-inline") \
  V(paddingInlineEnd, "padding-inline-end") \
  V(paddingInlineStart, "padding-inline-start") \
  V(paddingLeft, "padding-left") \
  V(paddingRight, "padding-right") \
  V(paddingTop, "padding-top") \
  V(page, "page") \
  V(pageBreakAfter, "page-break-after") \
  V(pageBreakBefore, "page-break-before") \
  V(pageBreakInside, "page-break-inside") \
  V(paintOrder, "paint-order") \
  V(perspective, "perspective") \
  V(perspectiveOrigin, "perspective-origin") \
  V(perspectiveOriginX, "perspective-origin-x") \
  V(perspectiveOriginY, "perspective-origin-y") \
  V(placeContent, "place-content") \
  V(placeItems, "place-items") \
  V(placeSelf, "place-self") \
  V(pointerEvents, "pointer-events") \
  V(position, "position") \
  V(printColorAdjust, "print-color-adjust") \
  V(quotes, "quotes") \
  V(r, "r") \
  V(resize, "resize") \
  V(right, "right") \
  V(rotate, "rotate") \
  V(rowGap, "row-gap") \
  V(rx, "rx") \
  V(ry, "ry") \
  V(scale, "scale") \
  V(scrollBehavior, "scroll-behavior") \
  V(scrollMargin, "scroll-margin") \
  V(scrollMarginBlock, "scroll-margin-block") \
  V(scrollMarginBlockEnd, "scroll-margin-block-end") \
  V(scrollMarginBlockStart, "scroll-margin-block-start") \
  V(scrollMarginBottom, "scroll-margin-bottom") \
  V(scrollMarginInline, "scroll-margin-inline") \
  V(scrollMarginInlineEnd, "scroll-margin-inline-end") \
  V(scrollMarginInlineStart, "scroll-margin-inline-start") \
  V(scrollMarginLeft, "scroll-margin-left") \
  V(scrollMarginRight, "scroll-margin-right") \
  V(scrollMarginTop, "scroll-margin-top") \
  V(scrollPadding, "scroll-padding") \
  V(scrollPaddingBlock, "scroll-padding-block") \
  V(scrollPaddingBlockEnd, "scroll-padding-block-end") \
  V(scrollPaddingBlockStart, "scroll-padding-block-start") \
  V(scrollPaddingBottom, "scroll-padding-bottom") \
  V(scrollPaddingInline, "scroll-padding-inline") \
  V(scrollPaddingInlineEnd, "scroll-padding-inline-end") \
  V(scrollPaddingInlineStart, "scroll-padding-inline-start") \
  V(scrollPaddingLeft, "scroll-padding-left") \
  V(scrollPaddingRight, "scroll-padding-right") \
  V(scrollPaddingTop, "scroll-padding-top") \
  V(scrollSnapAlign, "scroll-snap-align") \
  V(scrollSnapMargin, "scroll-snap-margin") \
  V(scrollSnapMarginBottom, "scroll-snap-margin-bottom") \
  V(scrollSnapMarginLeft, "scroll-snap-margin-left") \
  V(scrollSnapMarginRight, "scroll-snap-margin-right") \
  V(scrollSnapMarginTop, "scroll-snap-margin-top") \
  V(scrollSnapStop, "scroll-snap-stop") \
  V(scrollSnapType, "scroll-snap-type") \
  V(shapeImageThreshold, "shape-image-threshold") \
  V(shapeMargin, "shape-margin") \
  V(shapeOutside, "shape-outside") \
  V(shapeRendering, "shape-rendering") \
  V(size, "size") \
  V(speakAs, "speak-as") \
  V(stopColor, "stop-color") \
  V(stopOpacity, "stop-opacity") \
  V(stroke, "stroke") \
  V(strokeColor, "stroke-color") \
  V(strokeDasharray, "stroke-dasharray") \
  V(strokeDashoffset, "stroke-dashoffset") \
  V(strokeLinecap, "stroke-linecap") \
  V(strokeLinejoin, "stroke-linejoin") \
  V(strokeMiterlimit, "stroke-miterlimit") \
  V(strokeOpacity, "stroke-opacity") \
  V(strokeWidth, "stroke-width") \
  V(supportedColorSchemes, "supported-color-schemes") \
  V(tabSize, "tab-size") \
  V(tableLayout, "table-layout") \
  V(textAlign, "text-align") \
  V(textAlignLast, "text-align-last") \
  V(textAnchor, "text-anchor") \
  V(textCombineUpright, "text-combine-upright") \
  V(textDecoration, "text-decoration") \
  V(textDecorationColor, "text-decoration-color") \
  V(textDecorationLine, "text-decoration-line") \
  V(textDecorationSkip, "text-decoration-skip") \
  V(textDecorationSkipInk, "text-decoration-skip-ink") \
  V(textDecorationStyle, "text-decoration-style") \
  V(textDecorationThickness, "text-decoration-thickness") \
  V(textEdge, "text-edge") \
  V(textEmphasis, "text-emphasis") \
  V(textEmphasisColor, "text-emphasis-color") \
  V(textEmphasisPosition, "text-emphasis-position") \
  V(textEmphasisStyle, "text-emphasis-style") \
  V(textGroupAlign, "text-group-align") \
  V(textIndent, "text-indent") \
  V(textJustify, "text-justify") \
  V(textLineThrough, "text-line-through") \
  V(textLineThroughColor, "text-line-through-color") \
  V(textLineThroughMode, "text-line-through-mode") \
  V(textLineThroughStyle, "text-line-through-style") \
  V(textLineThroughWidth, "text-line-through-width") \
  V(textOrientation, "text-orientation") \
  V(textOverflow, "text-overflow") \
  V(textOverline, "text-overline") \
  V(textOverlineColor, "text-overline-color") \
  V(textOverlineMode, "text-overline-mode") \
  V(textOverlineStyle, "text-overline-style") \
  V(textOverlineWidth, "text-overline-width") \
  V(textRendering, "text-rendering") \
  V(textShadow, "text-shadow") \
  V(textTransform, "text-transform") \
  V(textUnderline, "text-underline") \
  V(textUnderlineColor, "text-underline-color") \
  V(textUnderlineMode, "text-underline-mode") \
  V(textUnderlineOffset, "text-underline-offset") \
  V(textUnderlinePosition, "text-underline-position") \
  V(textUnderlineStyle, "text-underline-style") \
  V(textUnderlineWidth, "text-underline-width") \
  V(textWrap, "text-wrap") \
  V(top, "top") \
  V(touchAction, "touch-action") \
  V(transform, "transform") \
  V(transformBox, "transform-box") \
  V(transformOrigin, "transform-origin") \
  V(transformOriginX, "transform-origin-x") \
  V(transformOriginY, "transform-origin-y") \
  V(transformOriginZ, "transform-origin-z") \
  V(transformStyle, "transform-style") \
  V(transition, "transition") \
  V(transitionDelay, "transition-delay") \
  V(transitionDuration, "transition-duration") \
  V(transitionProperty, "transition-property") \
  V(transitionTimingFunction, "transition-timing-function") \
  V(translate, "translate") \
  V(unicodeBidi, "unicode-bidi") \
  V(vectorEffect, "vector-effect") \
  V(verticalAlign, "vertical-align") \
  V(visibility, "visibility") \
  V(webkitAlignContent, "-webkit-align-content") \
  V(webkitAlignItems, "-webkit-align-items") \
  V(webkitAlignSelf, "-webkit-align-self") \
  V(webkitAlt, "-webkit-alt") \
  V(webkitAnimation, "-webkit-animation") \
  V(webkitAnimationDelay, "-webkit-animation-delay") \
  V(webkitAnimationDirection, "-webkit-animation-direction") \
  V(webkitAnimationDuration, "-webkit-animation-duration") \
  V(webkitAnimationFillMode, "-webkit-animation-fill-mode") \
  V(webkitAnimationIterationCount, "-webkit-animation-iteration-count") \
  V(webkitAnimationName, "-webkit-animation-name") \
  V(webkitAnimationPlayState, "-webkit-animation-play-state") \
  V(webkitAnimationTimingFunction, "-webkit-animation-timing-function") \
  V(webkitAppRegion, "-webkit-app-region") \
  V(webkitAppearance, "-webkit-appearance") \
  V(webkitBackdropFilter, "-webkit-backdrop-filter") \
  V(webkitBackfaceVisibility, "-webkit-backface-visibility") \
  V(webkitBackgroundClip, "-webkit-background-clip") \
  V(webkitBackgroundOrigin, "-webkit-background-origin") \
  V(webkitBackgroundSize, "-webkit-background-size") \
  V(webkitBorderAfter, "-webkit-border-after") \
  V(webkitBorderAfterColor, "-webkit-border-after-color") \
  V(webkitBorderAfterStyle, "-webkit-border-after-style") \
  V(webkitBorderAfterWidth, "-webkit-border-after-width") \
  V(webkitBorderBefore, "-webkit-border-before") \
  V(webkitBorderBeforeColor, "-webkit-border-before-color") \
  V(webkitBorderBeforeStyle, "-webkit-border-before-style") \
  V(webkitBorderBeforeWidth, "-webkit-border-before-width") \
  V(webkitBorderBottomLeftRadius, "-webkit-border-bottom-left-radius") \
  V(webkitBorderBottomRightRadius, "-webkit-border-bottom-right-radius") \
  V(webkitBorderEnd, "-webkit-border-end") \
  V(webkitBorderEndColor, "-webkit-border-end-color") \
  V(webkitBorderEndStyle, "-webkit-border-end-style") \
  V(webkitBorderEndWidth, "-webkit-border-end-width") \
  V(webkitBorderHorizontalSpacing, "-webkit-border-horizontal-spacing") \
  V(webkitBorderImage, "-webkit-border-image") \
  V(webkitBorderRadius, "-webkit-border-radius") \
  V(webkitBorderStart, "-webkit-border-start") \
  V(webkitBorderStartColor, "-webkit-border-start-color") \
  V(webkitBorderStartStyle, "-webkit-border-start-style") \
  V(webkitBorderStartWidth, "-webkit-border-start-width") \
  V(webkitBorderTopLeftRadius, "-webkit-border-top-left-radius") \
  V(webkitBorderTopRightRadius, "-webkit-border-top-right-radius") \
  V(webkitBorderVerticalSpacing, "-webkit-border-vertical-spacing") \
  V(webkitBoxAlign, "-webkit-box-align") \
  V(webkitBoxDecorationBreak, "-webkit-box-decoration-break") \
  V(webkitBoxDirection, "-webkit-box-direction") \
  V(webkitBoxFlex, "-webkit-box-flex") \
  V(webkitBoxFlexGroup, "-webkit-box-flex-group") \
  V(webkitBoxLines, "-webkit-box-lines") \
  V(webkitBoxOrdinalGroup, "-webkit-box-ordinal-group") \
  V(webkitBoxOrient, "-webkit-box-orient") \
  V(webkitBoxPack, "-webkit-box-pack") \
  V(webkitBoxReflect, "-webkit-box-reflect") \
  V(webkitBoxShadow, "-webkit-box-shadow") \
  V(webkitBoxSizing, "-webkit-box-sizing") \
  V(webkitBoxSnap, "-webkit-box-snap") \
  V(webkitClipPath, "-webkit-clip-path") \
  V(webkitColumnAxis, "-webkit-column-axis") \
  V(webkitColumnBreakAfter, "-webkit-column-break-after") \
  V(webkitColumnBreakBefore, "-webkit-column-break-before") \
  V(webkitColumnBreakInside, "-webkit-column-break-inside") \
  V(webkitColumnCount, "-webkit-column-count") \
  V(webkitColumnFill, "-webkit-column-fill") \
  V(webkitColumnGap, "-webkit-column-gap") \
  V(webkitColumnProgression, "-webkit-column-progression") \
  V(webkitColumnRule, "-webkit-column-rule") \
  V(webkitColumnRuleColor, "-webkit-column-rule-color") \
  V(webkitColumnRuleStyle, "-webkit-column-rule-style") \
  V(webkitColumnRuleWidth, "-webkit-column-rule-width") \
  V(webkitColumnSpan, "-webkit-column-span") \
  V(webkitColumnWidth, "-webkit-column-width") \
  V(webkitColumns, "-webkit-columns") \
  V(webkitCursorVisibility, "-webkit-cursor-visibility") \
  V(webkitFilter, "-webkit-filter") \
  V(webkitFlex, "-webkit-flex") \
  V(webkitFlexBasis, "-webkit-flex-basis") \
  V(webkitFlexDirection, "-webkit-flex-direction") \
  V(webkitFlexFlow, "-webkit-flex-flow") \
  V(webkitFlexGrow, "-webkit-flex-grow") \
  V(webkitFlexShrink, "-webkit-flex-shrink") \
  V(webkitFlexWrap, "-webkit-flex-wrap") \
  V(webkitFontKerning, "-webkit-font-kerning") \
  V(webkitFontSmoothing, "-webkit-font-smoothing") \
  V(webkitHyphenateCharacter, "-webkit-hyphenate-character") \
  V(webkitHyphenateLimitAfter, "-webkit-hyphenate-limit-after") \
  V(webkitHyphenateLimitBefore, "-webkit-hyphenate-limit-before") \
  V(webkitHyphenateLimitLines, "-webkit-hyphenate-limit-lines") \
  V(webkitHyphens, "-webkit-hyphens") \
  V(webkitInitialLetter, "-webkit-initial-letter") \
  V(webkitJustifyContent, "-webkit-justify-content") \
  V(webkitJustifyItems, "-webkit-justify-items") \
  V(webkitLineAlign, "-webkit-line-align") \
  V(webkitLineBoxContain, "-webkit-line-box-contain") \
  V(webkitLineBreak, "-webkit-line-break") \
  V(webkitLineClamp, "-webkit-line-clamp") \
  V(webkitLineGrid, "-webkit-line-grid") \
  V(webkitLineSnap, "-webkit-line-snap") \
  V(webkitLocale, "-webkit-locale") \
  V(webkitLogicalHeight, "-webkit-logical-height") \
  V(webkitLogicalWidth, "-webkit-logical-width") \
  V(webkitMarginAfter, "-webkit-margin-after") \
  V(webkitMarginBefore, "-webkit-margin-before") \
  V(webkitMarginEnd, "-webkit-margin-end") \
  V(webkitMarginStart, "-webkit-margin-start") \
  V(webkitMask, "-webkit-mask") \
  V(webkitMaskBoxImage, "-webkit-mask-box-image") \
  V(webkitMaskBoxImageOutset, "-webkit-mask-box-image-outset") \
  V(webkitMaskBoxImageRepeat, "-webkit-mask-box-image-repeat") \
  V(webkitMaskBoxImageSlice, "-webkit-mask-box-image-slice") \
  V(webkitMaskBoxImageSource, "-webkit-mask-box-image-source") \
  V(webkitMaskBoxImageWidth, "-webkit-mask-box-image-width") \
  V(webkitMaskClip, "-webkit-mask-clip") \
  V(webkitMaskComposite, "-webkit-mask-composite") \
  V(webkitMaskImage, "-webkit-mask-image") \
  V(webkitMaskOrigin, "-webkit-mask-origin") \
  V(webkitMaskPosition, "-webkit-mask-position") \
  V(webkitMaskPositionX, "-webkit-mask-position-x") \
  V(webkitMaskPositionY, "-webkit-mask-position-y") \
  V(webkitMaskRepeat, "-webkit-mask-repeat") \
  V(webkitMaskSize, "-webkit-mask-size") \
  V(webkitMaskSourceType, "-webkit-mask-source-type") \
  V(webkitMaxLogicalHeight, "-webkit-max-logical-height") \
  V(webkitMaxLogicalWidth, "-webkit-max-logical-width") \
  V(webkitMinLogicalHeight, "-webkit-min-logical-height") \
  V(webkitMinLogicalWidth, "-webkit-min-logical-width") \
  V(webkitNbspMode, "-webkit-nbsp-mode") \
  V(webkitOpacity, "-webkit-opacity") \
  V(webkitOrder, "-webkit-order") \
  V(webkitPaddingAfter, "-webkit-padding-after") \
  V(webkitPaddingBefore, "-webkit-padding-before") \
  V(webkitPaddingEnd, "-webkit-padding-end") \
  V(webkitPaddingStart, "-webkit-padding-start") \
  V(webkitPerspective, "-webkit-perspective") \
  V(webkitPerspectiveOrigin, "-webkit-perspective-origin") \
  V(webkitPerspectiveOriginX, "-webkit-perspective-origin-x") \
  V(webkitPerspectiveOriginY, "-webkit-perspective-origin-y") \
  V(webkitPrintColorAdjust, "-webkit-print-color-adjust") \
  V(webkitRtlOrdering, "-webkit-rtl-ordering") \
  V(webkitRubyPosition, "-webkit-ruby-position") \
  V(webkitShapeImageThreshold, "-webkit-shape-image-threshold") \
  V(webkitShapeMargin, "-webkit-shape-margin") \
  V(webkitShapeOutside, "-webkit-shape-outside") \
  V(webkitTextCombine, "-webkit-text-combine") \
  V(webkitTextDecoration, "-webkit-text-decoration") \
  V(webkitTextDecorationColor, "-webkit-text-decoration-color") \
  V(webkitTextDecorationLine, "-webkit-text-decoration-line") \
  V(webkitTextDecorationSkip, "-webkit-text-decoration-skip") \
  V(webkitTextDecorationStyle, "-webkit-text-decoration-style") \
  V(webkitTextDecorationsInEffect, "-webkit-text-decorations-in-effect") \
  V(webkitTextEmphasis, "-webkit-text-emphasis") \
  V(webkitTextEmphasisColor, "-webkit-text-emphasis-color") \
  V(webkitTextEmphasisPosition, "-webkit-text-emphasis-position") \
  V(webkitTextEmphasisStyle, "-webkit-text-emphasis-style") \
  V(webkitTextFillColor, "-webkit-text-fill-color") \
  V(webkitTextOrientation, "-webkit-text-orientation") \
  V(webkitTextSecurity, "-webkit-text-security") \
  V(webkitTextStroke, "-webkit-text-stroke") \
  V(webkitTextStrokeColor, "-webkit-text-stroke-color") \
  V(webkitTextStrokeWidth, "-webkit-text-stroke-width") \
  V(webkitTextUnderlinePosition, "-webkit-text-underline-position") \
  V(webkitTextZoom, "-webkit-text-zoom") \
  V(webkitTransform, "-webkit-transform") \
  V(webkitTransformOrigin, "-webkit-transform-origin") \
  V(webkitTransformOriginX, "-webkit-transform-origin-x") \
  V(webkitTransformOriginY, "-webkit-transform-origin-y") \
  V(webkitTransformOriginZ, "-webkit-transform-origin-z") \
  V(webkitTransformStyle, "-webkit-transform-style") \
  V(webkitTransition, "-webkit-transition") \
  V(webkitTransitionDelay, "-webkit-transition-delay") \
  V(webkitTransitionDuration, "-webkit-transition-duration") \
  V(webkitTransitionProperty, "-webkit-transition-property") \
  V(webkitTransitionTimingFunction, "-webkit-transition-timing-function") \
  V(webkitUserDrag, "-webkit-user-drag") \
  V(webkitUserModify, "-webkit-user-modify") \
  V(webkitUserSelect, "-webkit-user-select") \
  V(webkitWritingMode, "-webkit-writing-mode") \
  V(whiteSpace, "white-space") \
  V(widows, "widows") \
  V(width, "width") \
  V(willChange, "will-change") \
  V(wordBreak, "word-break") \
  V(wordSpacing, "word-spacing") \
  V(wordWrap, "word-wrap") \
  V(writingMode, "writing-mode") \
  V(x, "x") \
  V(y, "y") \
  V(zIndex, "z-index") \
  V(zoom, "zoom") \
  /* end */
/// \endcond

.. _Stonesense-dev-manual:

.. highlight:: cpp

#############################
Stonesense Development Manual
#############################

This is a guide for furthering the development of Stonesense.

Color Handling
==============

* ``uiColor()``

  A unified method for all the different color options. The syntax for this is:
  ``uiColor(int32_t index, bool bright)``, where:

  - ``index`` is a value from ``dfColors`` (such as ``dfColors::lgreen``).
  - ``bright`` (optional) allows changing the color to a lighter shade. If omitted, it defaults to ``false``.

Text Handling Methods
=====================

Getting Text Size
-----------------

* ``get_text_width()``

  This function takes either a ``char*`` or an ``ALLEGRO_USTR*`` and returns the width of the text in pixels
  as an ``int``.

* ``get_text_height()``

  This function calculates the height of the text, taking into account multi-line wrapping. It can accept a
  ``char*`` or an ``ALLEGRO_USTR*`` as input and optionally a ``maxWidth`` in pixels to account for wrapped text.

  **Parameters:**

  - ``text``: The text whose height is being calculated (``char*`` or ``ALLEGRO_USTR*``).
  - ``maxWidth``: Optional. The maximum width of the text in pixels. If omitted or set to a non-positive value,
  the height of a single line is returned.

  **Returns:**

  - The height of the text in pixels, accounting for line height and wrapping if applicable.

Text Wrapping and Drawing
-------------------------

* ``wrap_text()``

  This function wraps text into multiple lines based on the provided ``maxWidth`` and ``WrapMode``. It returns
  a list of strings, where each string represents a line of wrapped text.

  **Parameters:**

  - ``text``: The text to be wrapped.
  - ``maxWidth``: The maximum width for each line in pixels. If set to 0 or not specified, no wrapping occurs.
  - ``mode``: Defines the behavior when the text exceeds ``maxWidth``. See the ``WrapMode`` enum below for valid
  values.

  **Returns:**

  - A list of strings containing the wrapped lines.

* ``draw_multicolored_text()``

  This function draws multicolored text with support for wrapping. Each segment of text can have its own color,
  and the function will automatically adjust vertical positioning for each line.

  **Parameters:**

  - ``x``, ``y``: The starting position to draw the text.
  - ``flags``: Text flags (e.g., ``ALLEGRO_ALIGN_LEFT``, ``ALLEGRO_ALIGN_CENTER``) for alignment.
  - ``segments``: A list of pairs where each pair consists of a string (the text to display) and a color index
  (``int32_t``) from ``dfColors``. The color is applied to the corresponding text segment.
  - ``maxWidth``: Optional. Specifies the maximum width of the text before wrapping occurs. If omitted, the
  text is drawn as a single line.
  - ``mode``: Defines the behavior for wrapping the text when it exceeds ``maxWidth``. The default is ``MODE_NONE``.

  **Returns:**

  - None. This function is used for side effects, namely drawing the text on the screen.

Text Wrapping Mode
------------------

The ``WrapMode`` enum defines different behaviors for text when it exceeds the maximum width (``maxWidth``):

* ``MODE_TRUNCATE``: When the text exceeds ``maxWidth``, it is truncated, and a period (".") is appended to
indicate truncation. No further wrapping is performed.
* ``MODE_WRAP``: The text wraps to the next line when it exceeds ``maxWidth``.
* ``MODE_NONE``: No wrapping or truncation is performed. The text is rendered as a single line.

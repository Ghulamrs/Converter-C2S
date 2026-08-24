# The RStudio face of Converter-C2S.
#
# Two menu entries (Addins -> "Convert C <-> Shalimar", "Canonicalise C or
# Shalimar") and one plain function, all thin: the engine is the same C++14
# Converter class the command-line tool wraps, and it does no console work -
# every message arrives here as data and is shown R's way.

#' Convert between C89 and Shalimar.
#'
#' @param text   the source, as a single string or character vector of lines
#' @param name   a label for diagnostics, usually the file name; its
#'               extension picks the direction when `direction` is "auto"
#' @param direction "auto", "c-to-shalimar", "shalimar-to-c",
#'               "canon-c" or "canon-shalimar"
#' @return list(ok, output, beyond, questions, messages, summary)
#' @export
c2sConvert <- function(text, name = "input.c", direction = "auto") {
  if (length(text) > 1) text <- paste(text, collapse = "\n")
  .Call("c2s_convert_call", as.character(text), as.character(name),
        as.character(direction), PACKAGE = "c2sr")
}

c2sShow <- function(result, name) {
  for (m in result$messages) message(m)

  if (length(result$questions) > 0) {
    message(sprintf(
      "%d preprocessor construct(s) in '%s' need a decision before conversion:",
      length(result$questions), name))
    for (q in result$questions) message("  ", q)
    message("Decide each one - inline it, make it a variable or a function, ",
            "keep one branch, or remove it - then run the conversion again.")
    return(invisible(NULL))
  }

  if (!isTRUE(result$ok)) {
    message(result$summary)
    return(invisible(NULL))
  }

  if (result$beyond > 0) {
    message(sprintf("%d construct(s) marked #BEYOND SHALIMAR in the output.",
                    result$beyond))
  }

  if (requireNamespace("rstudioapi", quietly = TRUE) &&
      rstudioapi::isAvailable()) {
    rstudioapi::documentNew(result$output, type = "text")
  } else {
    cat(result$output)
  }
  invisible(result)
}

activeDocument <- function() {
  if (!requireNamespace("rstudioapi", quietly = TRUE) ||
      !rstudioapi::isAvailable()) {
    stop("this addin needs to run inside RStudio")
  }
  context <- rstudioapi::getSourceEditorContext()
  list(text = paste(context$contents, collapse = "\n"),
       name = if (nzchar(context$path)) basename(context$path) else "untitled.c")
}

#' Convert the active editor document; the Addins menu binds here.
#' @export
c2sConvertActiveDocument <- function() {
  doc <- activeDocument()
  c2sShow(c2sConvert(doc$text, doc$name, "auto"), doc$name)
}

#' Canonicalise the active editor document; the Addins menu binds here.
#' @export
c2sCanonicaliseActiveDocument <- function() {
  doc <- activeDocument()
  direction <- if (grepl("\\.(shm|shl)$", doc$name)) "canon-shalimar" else "canon-c"
  c2sShow(c2sConvert(doc$text, doc$name, direction), doc$name)
}

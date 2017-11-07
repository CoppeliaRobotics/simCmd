### Things left to do:

 - Actually execute the code (a proper simExecuteScriptString() is missing, and it is not possible to hack one using simSetScriptVariable() or simRegisterScriptVariable())
 - The list of script is not updated when adding a script to an object (perhaps no instancepass message is sent for that event)
 - Getting joint control callbacks scripts is not implemented (Plugin::onInstancePass in plugin.cpp)
 - History, like in bash/zsh
 - Smart completion like V-REP code editor (using QScintilla)
 - Improve the way of findinf the statusbar / attaching the lua commander widget below it, whenever V-REP provide a better way to do so (e.g. widget names, or an API for returning the staturbar widget pointer, or an API for adding something below the statusbar)
 - Add menu item to control widget's visibility
 - Persistently store widget's visibility setting


/**
 * Base file for tools
 */

/**
 * initialization, called when tool is called
 * Returning true will select tool and will make it possible to call work.
 * @param pl player_x object by whom the tool is called
 */
function init(pl)
{
	return true
}

/**
 * function called when the tool is clicked on a ground
 * the return string can have different meanings:
 * NULL: ok
 * "": unspecified error
 * "blabla": errors message, will be handled and translated as appropriate
 * @param pl player_x object by whom the tool is called
 * @param pos coord3d object that represents the position
 */
function work(pl, pos)
{
	return null
}

/**
 * function for position check. called before work()
 * @param pl player_x object by whom the tool is called
 * @param pos coord3d object that represents the position
 */
function check_pos(pl, pos)
{
	return null
}

/**
 * termination, called when the player exits from this tool
 * @param pl player_x object by whom the tool is called
 */
function exit(pl)
{
	return true
}
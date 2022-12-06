# Clockwork

Authors: David Lyons, Greg Loose, Yilin Wang

## Design

Clockwork is a game about writing code to control a party of robotic adventurers. In this prototype, you will be tasked with defeating two monsters in combat.

## How To Play

### Controls

Use your keyboard to type code into the game window. When you're done, press shift+enter to run your code, and watch the status messages to see how you did! You can also hold ctrl to speed up code execution.

### Language Specification

#### Basic Syntax

* You may only write one statement on each line. Currently, action statements, if statements, and while statements are supported, as well as the special END statement that terminates an if or while block.
* Do not end lines with a semicolon.
* Extra whitespace is allowed, and ignored.

#### Conditions

* Conditions currently suport ==, <, <=, >, >=, and != comparators.
* Single-value conditions are interpreted as "value != 0".
* TRUE and FALSE are interpreted as 1 and 0, respectively.
* Combining multiple conditions with AND and OR is not yet supported.

#### Object Properties

[object name] . [property name]

Example: IF (WARRIOR.HEALTH < WARRIOR.HEALTH_MAX)

These are variables specific to an object that may be used in place of any other value in a condition. Every object currently has the following properties:

* ALIVE: Boolean value which must be true for an object to perform any action.

* HEALTH: When this drops to 0, ALIVE is set to FALSE.

* HEALTH_MAX: Maximum value of the HEALTH property. Archer and wizard have 60, healer has 80, warrior has 100, Rupol has 160, and Fargoth has 240.

* DEFENSE: Reduces damage taken from attacks.

* POWER: (Warrior, archer, and monsters only) Determines damage dealt by attacking or shooting. Warrior, archer, and Fargoth have 20, Rupol has 30.

* ARROWS: (Archer only) When this drops to 0, the archer can no longer shoot. Starts at 8.

#### If statements

IF ( [condition] ) <br />
... <br />
END

Example:

IF (1 == 1) <br />
WARRIOR.ATTACK(FARGOTH) <br />
END

#### While statements

WHILE ( [condition] ) <br />
... <br />
END

Example:

WHILE (TRUE) <br />
WARRIOR.ATTACK(FARGOTH) <br />
END

#### Actions

[user object] . [action] ( [target object] )

Example: WARRIOR.ATTACK(FARGOTH)

#### Object Listing

##### WARRIOR

ATTACK: Deals damage to target.

##### WIZARD

FREEZE: The target is frozen and has a 1 in 3 chance to fail any action for the remainder of combat.

BURN: The target takes damage before every action for the rest of combat.

##### ARCHER

SHOOT: Deals damage to target. Can shoot twice per turn, but requires arrows to use. The archer starts with 8 arrows.

##### HEALER

HEAL: Partially restores the target's health. May not heal beyond maximum health.

##### FARGOTH

One of the enemies that you can attack. Attacks every unit before RUPOL, and has a lot of health.

##### RUPOL

One of the enemies that you can attack. Attacks every unit after FARGOTH, and does a lot of damage.

### Code Execution

In general, the player and the enemy take turns executing one line of code at a time. However, some statements take more or less than a full turn to execute. The conditional checks in an if or while statement only take a quarter of a turn, the archer's shoot action takes half a turn, and the wizard's spells take one and a half turns.

## Sources:

Font: [Roboto Mono](https://fonts.google.com/specimen/Roboto+Mono)
Some models and sounds: see dist/attribution.txt

This game was built with [NEST](NEST.md).


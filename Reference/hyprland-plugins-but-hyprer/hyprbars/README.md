# hyprbars

Adds simple title bars to windows.

![image](https://github.com/user-attachments/assets/184a66b9-eb91-4f6f-8953-b265a2735939)

## Config

All config options are in `plugin:hyprbars`:

```
plugin {
    hyprbars {
        # example config
        bar_height = 20
        bar_blur = 1

        # example buttons (R -> L)
        # hyprbars-button = color, size, on-click
        hyprbars-button = rgb(ff4040), 10, 󰖭, hyprctl dispatch killactive
        hyprbars-button = rgb(eeee11), 10, , hyprctl dispatch fullscreen 1

        # cmd to run on double click of the bar
        on_double_click = hyprctl dispatch fullscreen 1
    }
}
windowrulev2 = plugin:hyprbars:bar_height 10, ^floating:0
windowrulev2 = plugin:hyprbars:bar_blur 0, ^floating:0
windowrulev2 = plugin:hyprbars:hyprbars-title Kitty -- {OriginalTitle} -- {Date}, class:^(kitty)$
```
| property | type | description | default |
| --- | --- | --- | --- |
`enabled` | bool | whether to enable the bars |
`bar_color` | col | bar's background color |
`bar_height` | int | bar's height | `15` |
`bar_blur` | bool | whether to blur the bar. Also requires the global blur to be enabled. |
`col.text` | col | bar's title text color |
`bar_title_enabled` | bool | whether to render the title | `true` |
`bar_text_size` | int | bar's title text font size | `10` |
`bar_text_font` | str | bar's title text font | `Sans` |
`bar_text_align` | str | bar's title text alignment | `center`, can also be `left` |
`bar_buttons_alignment` | str | bar's buttons alignment | `right`, can also be `left` |
`bar_part_of_window` | bool | whether the bar is a part of the main window (if it is, stuff like shadows render around it) |
`bar_precedence_over_border` | bool | whether the bar should have a higher priority than the border (border will be around the bar) |
`bar_padding` | int | left / right edge padding | `7` |
`bar_button_padding` | int | padding between the buttons | `5` |
`icon_on_hover` | bool | whether the icons show on mouse hovering over the buttons | `false` |
`inactive_button_color` | col | buttons bg color when window isn't focused |
`on_double_click` | str | command to run on double click of the bar (not on a button) |

## Buttons Config

Use the `hyprbars-button` keyword.

```ini
hyprbars-button = bgcolor, size, icon, on-click, fgcolor
```

## Window rules

The Hyprer version of Hyprbars supports window rules for all of the above values (yes including the buttons), as well as adds a window rule for custom titles:

All of the plugins can be configured using this convention which changes the default value based on the window rule(example shown with non-floating window rule):

`windowrulev2 = plugin:hyprbars:bar_height 20, ^floating:0` -> (int) Changes the bar_height.

`windowrulev2 = plugin:hyprbars:bar_blur 1, ^floating:0` -> (bool) Changes the bar_blur.

`windowrulev2 = plugin:hyprbars:bar_color rgba(ff00007f), ^floating:0` -> (col) Changes the bar_color.

`windowrulev2 = plugin:hyprbars:bar_text_font Jetbrains Mono Nerd Font Mono Italic, ^floating:0` -> (str) Changes the bar_text_font. (Note that everything after plugin:hyprbars:bar_text_font but before the comma is passed as a single string)

The only exception to this rule is the enabled boolean. This one was created as nobar in the original plugin, so to maintain compatibility I kept it as it was.

`plugin:hyprbars:nobar` -> disables the bar on matching windows.

## Custom Titles

I also added custom title window rules. These titles can be formatted with variables. There are many variables, here are some examples:

`{Title}` -> The original title that would show by default.

`{Position}` -> The current position of the window on the screen.

`{Date}` -> The current date in %Y-%m-%d formatting.

`{Time}` -> The current time in %H:%M:%S formatting.

An example of this as a window rule would be:

`windowrulev2 = plugin:hyprbars:hyprbars-title Kitty -- {Title} -- {Date}, class:^(kitty)$`

I plan to add more variables, as well as ways to add your own. Currently, I have added most of the variables that can be viewed when running `hyprctl clients`, as well as a few other window variables. They can all be seen in the substituteTitleVars function at the top of `barDeco.cpp`.

One known issue with this is that the title won't always update unless the window is changed (for example, changing focus or moving it), so for something like the `{Time}` variable, it won't update automatically every second, so I plan to add a window rule to add a manual update interval.

## Button Window Rules

Defining Buttons in the original plugin involved passing multiple arguments, which makes things a little more difficult for window rules that only allow one argument to be passed before the window rule is applied. For this reason I had to pass all the arguments as one string, using a different delimeter than the comma. For this I used ">|<" as shown below.

`windowrulev2 = plugin:hyprbars:hyprbars_button rgba(ff0000ff)>|<10>|<X>|<hyprctl dispatch killactive>|<rgba(0000ffff), ^floating:0` -> Creates a button on only non-floating windows.
Any window that this matches will clear any buttons created the original way and only use the ones that use the window rule. For this reason, if you want a truly universal button (for example, a close button) you'll want to use `class:.*` or some other window rule that matches all windows.

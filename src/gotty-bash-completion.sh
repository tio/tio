_gotty()
{
    local cur prev opts base
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    #
    #  The options we'll complete.
    #
    opts="--baudrate \
          --databits \
          --flow \
          --stopbits \
          --parity \
          --no-autoconnect \
          --version \
          --help"


    #
    #  Complete the arguments to some of the options.
    #
    case "${prev}" in
        --baudrate)
            local baudrates="0 \
                             50 \
                             75 \
                             110 \
                             134 \
                             150 \
                             300 \
                             600 \
                             1200 \
                             2400 \
                             4800 \
                             9600 \
                             19200 \
                             38400 \
                             57600 \
                             115200 \
                             230400 \
                             460800 \
                             500000 \
                             576000 \
                             921600 \
                             1000000 \
                             1152000 \
                             1500000 \
                             2000000 \
                             2500000 \
                             3000000 \
                             3500000 \
                             4000000"
            COMPREPLY=( $(compgen -W "$baudrates" -- ${cur}) )
            return 0
            ;;
        --databits)
            COMPREPLY=( $(compgen -W "5 6 7 8" -- ${cur}) )
            return 0
            ;;
        --flow)
            COMPREPLY=( $(compgen -W "hard soft none" -- ${cur}) )
            return 0
            ;;
        --stopbits)
            COMPREPLY=( $(compgen -W "1 2" -- ${cur}) )
            return 0
            ;;
        --parity)
            COMPREPLY=( $(compgen -W "even odd none" -- ${cur}) )
            return 0
            ;;
        --no-autoconnect)
            COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
            return 0
            ;;
        --version)
            COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
            return 0
            ;;
        --help)
            COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
            return 0
            ;;
        *)
        ;;
    esac

   COMPREPLY=($(compgen -W "${opts}" -- ${cur}))  
   return 0
}
complete -o default -F _gotty gotty

zshz () {
        setopt LOCAL_OPTIONS NO_KSH_ARRAYS NO_SH_WORD_SPLIT EXTENDED_GLOB UNSET
        (( ZSHZ_DEBUG )) && setopt LOCAL_OPTIONS WARN_CREATE_GLOBAL
        local REPLY
        local -a lines
        local custom_datafile="${ZSHZ_DATA:-$_Z_DATA}" 
        if [[ -n ${custom_datafile} && ${custom_datafile} != */* ]]
        then
                print "ERROR: You configured a custom Zsh-z datafile (${custom_datafile}), but have not specified its directory." >&2
                exit
        fi
        local datafile=${${custom_datafile:-$HOME/.z}:A} 
        if [[ -d $datafile ]]
        then
                print "ERROR: Zsh-z's datafile (${datafile}) is a directory." >&2
                exit
        fi
        [[ -f $datafile ]] || {
                mkdir -p "${datafile:h}" && touch "$datafile"
        }
        [[ -z ${ZSHZ_OWNER:-${_Z_OWNER}} && -f $datafile && ! -O $datafile ]] && return
        lines=(${(f)"$(< $datafile)"}) 
        lines=(${(M)lines:#/*\|[[:digit:]]##[.,]#[[:digit:]]#\|[[:digit:]]##}) 
        _zshz_add_or_remove_path () {
                local action=${1} 
                shift
                if [[ $action == '--add' ]]
                then
                        [[ $* == $HOME ]] && return
                        local exclude
                        for exclude in ${(@)ZSHZ_EXCLUDE_DIRS:-${(@)_Z_EXCLUDE_DIRS}}
                        do
                                case $* in
                                        (${exclude} | ${exclude}/*) return ;;
                                esac
                        done
                fi
                local tempfile="${datafile}.${RANDOM}" 
                if (( ZSHZ[USE_FLOCK] ))
                then
                        local lockfd
                        zsystem flock -f lockfd "$datafile" 2> /dev/null || return
                fi
                integer tmpfd
                case $action in
                        (--add) exec {tmpfd}>| "$tempfile"
                                _zshz_update_datafile $tmpfd "$*"
                                local ret=$?  ;;
                        (--remove) local xdir
                                if (( ${ZSHZ_NO_RESOLVE_SYMLINKS:-${_Z_NO_RESOLVE_SYMLINKS}} ))
                                then
                                        [[ -d ${${*:-${PWD}}:a} ]] && xdir=${${*:-${PWD}}:a} 
                                else
                                        [[ -d ${${*:-${PWD}}:A} ]] && xdir=${${*:-${PWD}}:a} 
                                fi
                                local -a lines_to_keep
                                if (( ${+opts[-R]} ))
                                then
                                        if [[ $xdir == '/' ]] && ! read -q "?Delete entire Zsh-z database? "
                                        then
                                                print && return 1
                                        fi
                                        lines_to_keep=(${lines:#${xdir}\|*}) 
                                        lines_to_keep=(${lines_to_keep:#${xdir%/}/**}) 
                                else
                                        lines_to_keep=(${lines:#${xdir}\|*}) 
                                fi
                                if [[ $lines != "$lines_to_keep" ]]
                                then
                                        lines=($lines_to_keep) 
                                else
                                        return 1
                                fi
                                exec {tmpfd}>| "$tempfile"
                                print -u $tmpfd -l -- $lines
                                local ret=$?  ;;
                esac
                if (( tmpfd != 0 ))
                then
                        exec {tmpfd}>&-
                fi
                if (( ret != 0 ))
                then
                        ${ZSHZ[RM]} -f "$tempfile"
                        return $ret
                fi
                local owner
                owner=${ZSHZ_OWNER:-${_Z_OWNER}} 
                if (( ZSHZ[USE_FLOCK] ))
                then
                        ${ZSHZ[MV]} "$tempfile" "$datafile" 2> /dev/null || ${ZSHZ[RM]} -f "$tempfile"
                        if [[ -n $owner ]]
                        then
                                ${ZSHZ[CHOWN]} ${owner}:"$(id -ng ${owner})" "$datafile"
                        fi
                else
                        if [[ -n $owner ]]
                        then
                                ${ZSHZ[CHOWN]} "${owner}":"$(id -ng "${owner}")" "$tempfile"
                        fi
                        ${ZSHZ[MV]} -f "$tempfile" "$datafile" 2> /dev/null || ${ZSHZ[RM]} -f "$tempfile"
                fi
                if [[ $action == '--remove' ]]
                then
                        ZSHZ[DIRECTORY_REMOVED]=1 
                fi
        }
        _zshz_update_datafile () {
                integer fd=$1 
                local -A rank time
                local add_path=${(q)2} 
                local -a existing_paths
                local now=$EPOCHSECONDS line dir 
                local path_field rank_field time_field count x
                rank[$add_path]=1 
                time[$add_path]=$now 
                for line in $lines
                do
                        if [[ ! -d ${line%%\|*} ]]
                        then
                                for dir in ${(@)ZSHZ_KEEP_DIRS}
                                do
                                        if [[ ${line%%\|*} == ${dir}/* || ${line%%\|*} == $dir || $dir == '/' ]]
                                        then
                                                existing_paths+=($line) 
                                        fi
                                done
                        else
                                existing_paths+=($line) 
                        fi
                done
                lines=($existing_paths) 
                for line in $lines
                do
                        path_field=${(q)line%%\|*} 
                        rank_field=${${line%\|*}#*\|} 
                        time_field=${line##*\|} 
                        (( rank_field < 1 )) && continue
                        if [[ $path_field == $add_path ]]
                        then
                                rank[$path_field]=$rank_field 
                                (( rank[$path_field]++ ))
                                time[$path_field]=$now 
                        else
                                rank[$path_field]=$rank_field 
                                time[$path_field]=$time_field 
                        fi
                        (( count += rank_field ))
                done
                if (( count > ${ZSHZ_MAX_SCORE:-${_Z_MAX_SCORE:-9000}} ))
                then
                        for x in ${(k)rank}
                        do
                                print -u $fd -- "$x|$(( 0.99 * rank[$x] ))|${time[$x]}" || return 1
                        done
                else
                        for x in ${(k)rank}
                        do
                                print -u $fd -- "$x|${rank[$x]}|${time[$x]}" || return 1
                        done
                fi
        }
        _zshz_legacy_complete () {
                local line path_field path_field_normalized
                1=${1//[[:space:]]/*} 
                for line in $lines
                do
                        path_field=${line%%\|*} 
                        path_field_normalized=$path_field 
                        if (( ZSHZ_TRAILING_SLASH ))
                        then
                                path_field_normalized=${path_field%/}/ 
                        fi
                        if [[ $1 == "${1:l}" && ${path_field_normalized:l} == *${~1}* ]]
                        then
                                print -- $path_field
                        elif [[ $path_field_normalized == *${~1}* ]]
                        then
                                print -- $path_field
                        fi
                done
        }
        _zshz_printv () {
                if (( ZSHZ[PRINTV] ))
                then
                        builtin print -v REPLY -f %s $@
                else
                        builtin print -z $@
                        builtin read -rz REPLY
                fi
        }
        _zshz_find_common_root () {
                local -a common_matches
                local x short
                common_matches=(${(@Pk)1}) 
                for x in ${(@)common_matches}
                do
                        if [[ -z $short ]] || (( $#x < $#short )) || [[ $x != ${short}/* ]]
                        then
                                short=$x 
                        fi
                done
                [[ $short == '/' ]] && return
                for x in ${(@)common_matches}
                do
                        [[ $x != $short* ]] && return
                done
                _zshz_printv -- $short
        }
        _zshz_output () {
                local match_array=$1 match=$2 format=$3 
                local common k x
                local -a descending_list output
                local -A output_matches
                output_matches=(${(Pkv)match_array}) 
                _zshz_find_common_root $match_array
                common=$REPLY 
                case $format in
                        (completion) for k in ${(@k)output_matches}
                                do
                                        _zshz_printv -f "%.2f|%s" ${output_matches[$k]} $k
                                        descending_list+=(${(f)REPLY}) 
                                        REPLY='' 
                                done
                                descending_list=(${${(@On)descending_list}#*\|}) 
                                print -l $descending_list ;;
                        (list) local path_to_display
                                for x in ${(k)output_matches}
                                do
                                        if (( ${output_matches[$x]} ))
                                        then
                                                path_to_display=$x 
                                                (( ZSHZ_TILDE )) && path_to_display=${path_to_display/#${HOME}/\~} 
                                                _zshz_printv -f "%-10d %s\n" ${output_matches[$x]} $path_to_display
                                                output+=(${(f)REPLY}) 
                                                REPLY='' 
                                        fi
                                done
                                if [[ -n $common ]]
                                then
                                        (( ZSHZ_TILDE )) && common=${common/#${HOME}/\~} 
                                        (( $#output > 1 )) && printf "%-10s %s\n" 'common:' $common
                                fi
                                if (( $+opts[-t] ))
                                then
                                        for x in ${(@On)output}
                                        do
                                                print -- $x
                                        done
                                elif (( $+opts[-r] ))
                                then
                                        for x in ${(@on)output}
                                        do
                                                print -- $x
                                        done
                                else
                                        for x in ${(@on)output}
                                        do
                                                print $x
                                        done
                                fi ;;
                        (*) if (( ! ZSHZ_UNCOMMON )) && [[ -n $common ]]
                                then
                                        _zshz_printv -- $common
                                else
                                        _zshz_printv -- ${(P)match}
                                fi ;;
                esac
        }
        _zshz_find_matches () {
                setopt LOCAL_OPTIONS NO_EXTENDED_GLOB
                local fnd=$1 method=$2 format=$3 
                local -a existing_paths
                local line dir path_field rank_field time_field rank dx escaped_path_field
                local -A matches imatches
                local best_match ibest_match hi_rank=-9999999999 ihi_rank=-9999999999 
                for line in $lines
                do
                        if [[ ! -d ${line%%\|*} ]]
                        then
                                for dir in ${(@)ZSHZ_KEEP_DIRS}
                                do
                                        if [[ ${line%%\|*} == ${dir}/* || ${line%%\|*} == $dir || $dir == '/' ]]
                                        then
                                                existing_paths+=($line) 
                                        fi
                                done
                        else
                                existing_paths+=($line) 
                        fi
                done
                lines=($existing_paths) 
                for line in $lines
                do
                        path_field=${line%%\|*} 
                        rank_field=${${line%\|*}#*\|} 
                        time_field=${line##*\|} 
                        case $method in
                                (rank) rank=$rank_field  ;;
                                (time) (( rank = time_field - EPOCHSECONDS )) ;;
                                (*) (( dx = EPOCHSECONDS - time_field ))
                                        rank=$(( 10000 * rank_field * (3.75/( (0.0001 * dx + 1) + 0.25)) ))  ;;
                        esac
                        local q=${fnd//[[:space:]]/\*} 
                        local path_field_normalized=$path_field 
                        if (( ZSHZ_TRAILING_SLASH ))
                        then
                                path_field_normalized=${path_field%/}/ 
                        fi
                        if [[ $ZSHZ_CASE == 'smart' && ${1:l} == $1 && ${path_field_normalized:l} == ${~q:l} ]]
                        then
                                imatches[$path_field]=$rank 
                        elif [[ $ZSHZ_CASE != 'ignore' && $path_field_normalized == ${~q} ]]
                        then
                                matches[$path_field]=$rank 
                        elif [[ $ZSHZ_CASE != 'smart' && ${path_field_normalized:l} == ${~q:l} ]]
                        then
                                imatches[$path_field]=$rank 
                        fi
                        escaped_path_field=${path_field//'\'/'\\'} 
                        escaped_path_field=${escaped_path_field//'`'/'\`'} 
                        escaped_path_field=${escaped_path_field//'('/'\('} 
                        escaped_path_field=${escaped_path_field//')'/'\)'} 
                        escaped_path_field=${escaped_path_field//'['/'\['} 
                        escaped_path_field=${escaped_path_field//']'/'\]'} 
                        if (( matches[$escaped_path_field] )) && (( matches[$escaped_path_field] > hi_rank ))
                        then
                                best_match=$path_field 
                                hi_rank=${matches[$escaped_path_field]} 
                        elif (( imatches[$escaped_path_field] )) && (( imatches[$escaped_path_field] > ihi_rank ))
                        then
                                ibest_match=$path_field 
                                ihi_rank=${imatches[$escaped_path_field]} 
                                ZSHZ[CASE_INSENSITIVE]=1 
                        fi
                done
                [[ -z $best_match && -z $ibest_match ]] && return 1
                if [[ -n $best_match ]]
                then
                        _zshz_output matches best_match $format
                elif [[ -n $ibest_match ]]
                then
                        _zshz_output imatches ibest_match $format
                fi
        }
        local -A opts
        zparseopts -E -D -A opts -- -add -complete c e h -help l r R t x
        if [[ $1 == '--' ]]
        then
                shift
        elif [[ -n ${(M)@:#-*} && -z $compstate ]]
        then
                print "Improper option(s) given."
                _zshz_usage
                return 1
        fi
        local opt output_format method='frecency' fnd prefix req 
        for opt in ${(k)opts}
        do
                case $opt in
                        (--add) [[ ! -d $* ]] && return 1
                                local dir
                                if [[ $OSTYPE == (cygwin|msys) && $PWD == '/' && $* != /* ]]
                                then
                                        set -- "/$*"
                                fi
                                if (( ${ZSHZ_NO_RESOLVE_SYMLINKS:-${_Z_NO_RESOLVE_SYMLINKS}} ))
                                then
                                        dir=${*:a} 
                                else
                                        dir=${*:A} 
                                fi
                                _zshz_add_or_remove_path --add "$dir"
                                return ;;
                        (--complete) if [[ -s $datafile && ${ZSHZ_COMPLETION:-frecent} == 'legacy' ]]
                                then
                                        _zshz_legacy_complete "$1"
                                        return
                                fi
                                output_format='completion'  ;;
                        (-c) [[ $* == ${PWD}/* || $PWD == '/' ]] || prefix="$PWD "  ;;
                        (-h | --help) _zshz_usage
                                return ;;
                        (-l) output_format='list'  ;;
                        (-r) method='rank'  ;;
                        (-t) method='time'  ;;
                        (-x) if [[ $OSTYPE == (cygwin|msys) && $PWD == '/' && $* != /* ]]
                                then
                                        set -- "/$*"
                                fi
                                _zshz_add_or_remove_path --remove $*
                                return ;;
                esac
        done
        req="$*" 
        fnd="$prefix$*" 
        [[ -n $fnd && $fnd != "$PWD " ]] || {
                [[ $output_format != 'completion' ]] && output_format='list' 
        }
        zshz_cd () {
                setopt LOCAL_OPTIONS NO_WARN_CREATE_GLOBAL
                if [[ -z $ZSHZ_CD ]]
                then
                        builtin cd "$*"
                else
                        ${=ZSHZ_CD} "$*"
                fi
        }
        _zshz_echo () {
                if (( ZSHZ_ECHO ))
                then
                        if (( ZSHZ_TILDE ))
                        then
                                print ${PWD/#${HOME}/\~}
                        else
                                print $PWD
                        fi
                fi
        }
        if [[ ${@: -1} == /* ]] && (( ! $+opts[-e] && ! $+opts[-l] ))
        then
                [[ -d ${@: -1} ]] && zshz_cd ${@: -1} && _zshz_echo && return
        fi
        if [[ ! -z ${(tP)opts[-c]} ]]
        then
                _zshz_find_matches "$fnd*" $method $output_format
        else
                _zshz_find_matches "*$fnd*" $method $output_format
        fi
        local ret2=$? 
        local cd
        cd=$REPLY 
        if (( ZSHZ_UNCOMMON )) && [[ -n $cd ]]
        then
                if [[ -n $cd ]]
                then
                        local q=${fnd//[[:space:]]/\*} 
                        q=${q%/} 
                        if (( ! ZSHZ[CASE_INSENSITIVE] ))
                        then
                                local q_chars=$(( ${#cd} - ${#${cd//${~q}/}} )) 
                                until (( ( ${#cd:h} - ${#${${cd:h}//${~q}/}} ) != q_chars ))
                                do
                                        cd=${cd:h} 
                                done
                        else
                                local q_chars=$(( ${#cd} - ${#${${cd:l}//${~${q:l}}/}} )) 
                                until (( ( ${#cd:h} - ${#${${${cd:h}:l}//${~${q:l}}/}} ) != q_chars ))
                                do
                                        cd=${cd:h} 
                                done
                        fi
                        ZSHZ[CASE_INSENSITIVE]=0 
                fi
        fi
        if (( ret2 == 0 )) && [[ -n $cd ]]
        then
                if (( $+opts[-e] ))
                then
                        (( ZSHZ_TILDE )) && cd=${cd/#${HOME}/\~} 
                        print -- "$cd"
                else
                        [[ -d $cd ]] && zshz_cd "$cd" && _zshz_echo
                fi
        else
                if ! (( $+opts[-e] || $+opts[-l] )) && [[ -d $req ]]
                then
                        zshz_cd "$req" && _zshz_echo
                else
                        return $ret2
                fi
        fi
}

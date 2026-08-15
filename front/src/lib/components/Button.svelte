<script lang="ts">
	import type { Snippet } from 'svelte';

	type ButtonVariant = 'primary' | 'ghost' | 'danger' | 'warning' | 'info' | 'success';

	interface Props {
		tag?: 'button' | 'a';
		variant?: ButtonVariant;
		href?: string;
		children: Snippet;
		customClass?: string;
		isIcon?: boolean;
		[key: string]: unknown;
	}

	let {
		tag = 'button',
		isIcon = false,
		variant,
		children,
		customClass,
		...props
	}: Props = $props();
</script>

<svelte:element
	this={tag}
	class={`button ${variant ? `button--${variant}` : ''} ${customClass ? customClass : ''} ${isIcon ? 'button--icon' : ''}`}
	{...props}
>
	{@render children()}
</svelte:element>

<style lang="scss">
	@use './../styles/variables' as *;

	.button {
		background: rgba($white, 0.9);
		display: flex;
		justify-content: center;
		align-items: center;
		padding: 8px 16px;
		border-radius: $border-radius;
		cursor: pointer;
		transition: all 0.3s ease;
		border: 0;
		width: var(--width, fit-content);
		font-weight: 500;
		margin: 0;
		line-height: 1.5;

		&:hover,
		&:focus {
			background: $white;
		}

		&--primary {
			background: rgba($blue-dark, 0.9);
			color: $white;

			&:hover,
			&:focus {
				background: $blue-dark;
			}
		}

		&--ghost {
			background: transparent;
			color: $black;

			&:hover,
			&:focus {
				background: rgba($black, 0.1);
			}
		}

		&--icon {
			padding: 8px;
		}
	}
</style>
